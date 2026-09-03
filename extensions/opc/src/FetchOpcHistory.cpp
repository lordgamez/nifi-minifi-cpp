/**
 * Licensed to the Apache Software Foundation (ASF) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.
 * The ASF licenses this file to You under the Apache License, Version 2.0
 * (the "License"); you may not use this file except in compliance with
 * the License.  You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "FetchOpcHistory.h"

#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "core/ProcessSession.h"
#include "core/Resource.h"
#include "minifi-cpp/core/ProcessContext.h"
#include "rapidjson/document.h"
#include "rapidjson/rapidjson.h"
#include "rapidjson/stringbuffer.h"
#include "rapidjson/writer.h"
#include "utils/ProcessorConfigUtils.h"
#include "utils/StringUtils.h"
#include "utils/TimeUtil.h"

namespace org::apache::nifi::minifi::processors {

namespace {

constexpr const char* LAST_FETCHED_TIMESTAMP_KEY = "last_fetched_timestamp";
constexpr const char* LAST_FETCHED_FINGERPRINT_KEY = "last_fetched_fingerprint";

std::string updateTypeToString(UA_HistoryUpdateType type) {
  switch (type) {
    case UA_HISTORYUPDATETYPE_INSERT:
      return "Insert";
    case UA_HISTORYUPDATETYPE_REPLACE:
      return "Replace";
    case UA_HISTORYUPDATETYPE_UPDATE:
      return "Update";
    case UA_HISTORYUPDATETYPE_DELETE:
      return "Delete";
    default:
      return "Unknown";
  }
}

std::string uaStringToString(const UA_String& str) {
  return std::string(reinterpret_cast<const char*>(str.data), str.length);
}

struct HistoryEntry {
  std::string value;
  int64_t source_timestamp = 0;
  const UA_ModificationInfo* modification_info = nullptr;

  int64_t modificationTime() const {
    return modification_info ? modification_info->modificationTime : UA_DateTime_fromUnixTime(0);
  }
};

struct HistoryBatch {
  std::vector<HistoryEntry> entries;
  bool has_modification_info = false;
};

struct Fingerprint {
  int64_t source_timestamp = 0;
  std::optional<int64_t> modification_timestamp;
  std::string value;
};

std::optional<HistoryBatch> extractHistoryBatch(const UA_ExtensionObject* data) {
  const UA_DataValue* data_values = nullptr;
  size_t data_value_size = 0;
  const UA_ModificationInfo* modification_infos = nullptr;
  size_t modification_infos_size = 0;

  if (data->content.decoded.type == &UA_TYPES[UA_TYPES_HISTORYDATA]) {
    const auto* history_data = static_cast<const UA_HistoryData*>(data->content.decoded.data);
    data_values = history_data->dataValues;
    data_value_size = history_data->dataValuesSize;
  } else if (data->content.decoded.type == &UA_TYPES[UA_TYPES_HISTORYMODIFIEDDATA]) {
    const auto* modified_data = static_cast<const UA_HistoryModifiedData*>(data->content.decoded.data);
    data_values = modified_data->dataValues;
    data_value_size = modified_data->dataValuesSize;
    modification_infos = modified_data->modificationInfos;
    modification_infos_size = modified_data->modificationInfosSize;
  } else {
    // TODO: Unexpected data type received in the callback, how to handle this?
    return std::nullopt;
  }

  HistoryBatch batch;
  batch.has_modification_info = modification_infos != nullptr;
  batch.entries.reserve(data_value_size);
  for (size_t i = 0; i < data_value_size; ++i) {
    HistoryEntry entry;
    try {
      entry.value = opc::variantToString(data_values[i].value);
    } catch (const opc::OPCException&) {
      // Unsupported value type: leave content empty. An exception must not unwind across the C history-read callback boundary in open62541.
      // TODO: Log a warning about the unsupported value type.
    }
    entry.source_timestamp = data_values[i].sourceTimestamp;
    // modificationInfos is parallel to dataValues by index (OPC UA Part 11):
    // entry i describes value i, or is absent if the value was never edited.
    entry.modification_info = (modification_infos && i < modification_infos_size) ? &modification_infos[i] : nullptr;
    batch.entries.push_back(std::move(entry));
  }
  return batch;
}

std::optional<Fingerprint> parseFingerprint(const std::unordered_map<std::string, std::string>& state_map, bool has_modification_info) {
  const auto it = state_map.find(LAST_FETCHED_FINGERPRINT_KEY);
  if (it == state_map.end()) {
    return std::nullopt;
  }

  const auto parts = utils::string::split(it->second, ":");
  Fingerprint fingerprint;
  if (has_modification_info) {
    if (parts.size() != 3) {
      return std::nullopt;  // TODO: warning
    }
    fingerprint.source_timestamp = std::stoll(parts[0]);
    fingerprint.modification_timestamp = std::stoll(parts[1]);
    fingerprint.value = parts[2];
  } else {
    if (parts.size() != 2) {
      return std::nullopt;  // TODO: warning
    }
    fingerprint.source_timestamp = std::stoll(parts[0]);
    fingerprint.value = parts[1];
  }
  return fingerprint;
}

// Drops the entries that were already emitted on a previous read. Several entries may share the last-fetched
// source timestamp, so entries at that timestamp are skipped until the previously-emitted one has been passed.
std::vector<HistoryEntry> selectNewEntries(std::vector<HistoryEntry> entries, const std::optional<Fingerprint>& last_fetched) {
  if (!last_fetched) {
    return entries;
  }

  std::vector<HistoryEntry> new_entries;
  new_entries.reserve(entries.size());
  bool already_fetched_found = false;
  for (auto& entry : entries) {
    if (!already_fetched_found && entry.source_timestamp == last_fetched->source_timestamp) {
      const bool matches = last_fetched->modification_timestamp
          ? entry.modificationTime() == *last_fetched->modification_timestamp && entry.value == last_fetched->value
          : entry.value == last_fetched->value;
      if (matches) {
        already_fetched_found = true;
      }
      continue;
    }
    new_entries.push_back(std::move(entry));
  }
  return new_entries;
}

void addModificationInfo(core::Record& record, const UA_ModificationInfo& modification_info) {
  if (modification_info.userName.length > 0) {
    record.emplace("ModificationUsername", core::RecordField(uaStringToString(modification_info.userName)));
  }
  record.emplace("ModificationTime", core::RecordField(opc::OPCDateTime2String(modification_info.modificationTime)));
  record.emplace("ModificationUpdateType", core::RecordField(updateTypeToString(modification_info.updateType)));
}

void addModificationInfo(core::FlowFile& flow_file, const UA_ModificationInfo& modification_info) {
  if (modification_info.userName.length > 0) {
    flow_file.addAttribute("ModificationUsername", uaStringToString(modification_info.userName));
  }
  flow_file.addAttribute("ModificationTime", opc::OPCDateTime2String(modification_info.modificationTime));
  flow_file.addAttribute("ModificationUpdateType", updateTypeToString(modification_info.updateType));
}

core::Record toRecord(const std::string& node_id, const HistoryEntry& entry) {
  core::Record record;
  record.emplace("Value", core::RecordField(std::string(entry.value)));
  record.emplace("NodeID", core::RecordField(node_id));
  record.emplace("Sourcetimestamp", core::RecordField(opc::OPCDateTime2String(entry.source_timestamp)));
  if (entry.modification_info) {
    addModificationInfo(record, *entry.modification_info);
  }
  return record;
}

// Emits all new entries as a single FlowFile written through the configured record set writer.
void writeAsRecordSet(FetchOpcHistoryContext& context, const std::vector<HistoryEntry>& entries) {
  core::RecordSet record_set;
  for (const auto& entry : entries) {
    record_set.push_back(toRecord(context.node_id, entry));
  }

  auto flow_file = context.session.create();
  context.record_set_writer->write(record_set, flow_file, context.session);
  context.session.transfer(flow_file, FetchOpcHistory::Success);
  ++context.flow_files_transferred;
}

// Emits each new entry as its own FlowFile whose content is the raw value.
void writeAsFlowFiles(FetchOpcHistoryContext& context, const std::vector<HistoryEntry>& entries) {
  for (const auto& entry : entries) {
    auto flow_file = context.session.create();
    context.session.write(flow_file, [&entry](const std::shared_ptr<io::OutputStream>& output_stream) -> io::IoResult {
      output_stream->write(reinterpret_cast<const uint8_t*>(entry.value.data()), entry.value.size());
      return io::IoResult::from(entry.value.size());
    });
    flow_file->addAttribute("NodeID", context.node_id);
    flow_file->addAttribute("Sourcetimestamp", opc::OPCDateTime2String(entry.source_timestamp));
    if (entry.modification_info) {
      addModificationInfo(*flow_file, *entry.modification_info);
    }
    context.session.transfer(flow_file, FetchOpcHistory::Success);
    ++context.flow_files_transferred;
  }
}

void updateState(std::unordered_map<std::string, std::string>& state_map, const HistoryEntry& last_entry) {
  std::string fingerprint = std::to_string(last_entry.source_timestamp) + ":";
  if (last_entry.modification_info && last_entry.modification_info->modificationTime > 0) {
    fingerprint += std::to_string(last_entry.modification_info->modificationTime) + ":";
  }
  fingerprint += last_entry.value;
  state_map[LAST_FETCHED_TIMESTAMP_KEY] = std::to_string(last_entry.source_timestamp);
  state_map[LAST_FETCHED_FINGERPRINT_KEY] = fingerprint;
}

}  // namespace

void FetchOpcHistory::initialize() {
  setSupportedProperties(Properties);
  setSupportedRelationships(Relationships);
}

void FetchOpcHistory::onSchedule(core::ProcessContext& context, core::ProcessSessionFactory& factory) {
  logger_->log_trace("FetchOpcHistory::onSchedule");
  BaseOPCProcessor::onSchedule(context, factory);
  node_id_ = utils::parseProperty(context, NodeID);
  parseIdType(context, NodeIDType);
  namespace_idx_ = gsl::narrow<int32_t>(utils::parseI64Property(context, NameSpaceIndex));

  history_type_ = utils::parseEnumProperty<opc::HistoryReadTypeOption>(context, HistoryReadType);
  start_timestamp_ = utils::parseOptionalProperty(context, StartTimestamp) | utils::andThen(utils::timeutils::parseDateTimeStr);
  end_timestamp_ = utils::parseOptionalProperty(context, EndTimestamp) | utils::andThen(utils::timeutils::parseDateTimeStr);
  batch_size_ = utils::parseOptionalU64Property(context, BatchSize).value_or(0);
  const auto record_set_writer_name = context.getProperty(RecordSetWriter).value_or("");
  record_set_writer_ = std::dynamic_pointer_cast<core::RecordSetWriter>(context.getControllerService(record_set_writer_name, getUUID()));
}

UA_Boolean FetchOpcHistory::historyReadCallback(UA_Client* /*client*/, const UA_NodeId* /*node_id*/, UA_Boolean more_data_available,
    const UA_ExtensionObject* data, void* ctx) {
  auto* opc_history_context = static_cast<FetchOpcHistoryContext*>(ctx);
  opc_history_context->has_more_data = more_data_available;

  auto batch = extractHistoryBatch(data);
  if (!batch || batch->entries.empty()) {
    return false;
  }

  const auto last_fetched = parseFingerprint(opc_history_context->state_map, batch->has_modification_info);
  const auto new_entries = selectNewEntries(std::move(batch->entries), last_fetched);
  if (new_entries.empty()) {
    return false;
  }

  if (opc_history_context->record_set_writer) {
    writeAsRecordSet(*opc_history_context, new_entries);
  } else {
    writeAsFlowFiles(*opc_history_context, new_entries);
  }

  updateState(opc_history_context->state_map, new_entries.back());
  return false;
}

void FetchOpcHistory::onTrigger(core::ProcessContext& context, core::ProcessSession& session) {
  logger_->log_trace("FetchOpcHistory::onTrigger");

  if (!reconnect()) {
    context.yield();
    return;
  }

  auto* state_manager = context.getStateManager();
  std::unordered_map<std::string, std::string> state_map;

  // TODO: handle previous state to avoid fetching the same history entries multiple times. This may require storing the last fetched timestamp or
  // entry ID in the state manager.
  state_manager->get(state_map);

  UA_NodeId node = UA_NODEID_STRING(namespace_idx_, const_cast<char*>(node_id_.c_str()));
  bool has_more_data = true;
  size_t flow_files_transferred = 0;
  FetchOpcHistoryContext history_context{session, record_set_writer_, state_map, has_more_data, flow_files_transferred, node_id_};

  UA_DateTime ua_start_time = UA_DateTime_fromUnixTime(0);
  UA_DateTime ua_end_time = UA_DateTime_now();
  if (state_map.find(LAST_FETCHED_TIMESTAMP_KEY) != state_map.end()) {
    ua_start_time = std::stoll(state_map[LAST_FETCHED_TIMESTAMP_KEY].c_str());
  } else if (start_timestamp_.has_value()) {
    uint64_t start_time_seconds = std::chrono::duration_cast<std::chrono::seconds>(start_timestamp_->time_since_epoch()).count();
    ua_start_time = UA_DateTime_fromUnixTime(start_time_seconds);
  }

  if (end_timestamp_.has_value()) {
    uint64_t end_time_seconds = std::chrono::duration_cast<std::chrono::seconds>(end_timestamp_->time_since_epoch()).count();
    ua_end_time = UA_DateTime_fromUnixTime(end_time_seconds);
  }

  auto number_of_entries_to_fetch = batch_size_;
  while (has_more_data && (batch_size_ == 0 || flow_files_transferred < batch_size_)) {
    auto retval = connection_->readHistory(history_type_,
        node,
        &FetchOpcHistory::historyReadCallback,
        ua_start_time,
        ua_end_time,
        number_of_entries_to_fetch,
        (void*)&history_context);

    if (retval != UA_STATUSCODE_GOOD) {
      // TODO: handle error, possibly yield and log the error
      break;
    }
    number_of_entries_to_fetch *= 2;
  }

  state_manager->set(state_map);
}

REGISTER_RESOURCE(FetchOpcHistory, Processor);

}  // namespace org::apache::nifi::minifi::processors
