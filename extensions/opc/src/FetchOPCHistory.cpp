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

#include "FetchOPCHistory.h"

#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
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

struct FetchedState {
  int64_t timestamp = 0;
  std::unordered_set<std::string> fingerprints;
};

std::string entryFingerprint(const HistoryEntry& entry, bool has_modification_info) {
  std::string raw = ":" + entry.value;
  if (has_modification_info) {
    raw = std::to_string(entry.modificationTime()) + raw;
  }
  return utils::string::to_hex(raw);
}

std::optional<HistoryBatch> extractHistoryBatch(const UA_ExtensionObject* data, const std::shared_ptr<core::logging::Logger>& logger) {
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
    logger->log_error("Unexpected data type received in the history read callback: {}", data->content.decoded.type->typeName);
    return std::nullopt;
  }

  HistoryBatch batch;
  batch.has_modification_info = modification_infos != nullptr;
  batch.entries.reserve(data_value_size);
  for (size_t i = 0; i < data_value_size; ++i) {
    HistoryEntry entry;
    try {
      entry.value = opc::variantToString(data_values[i].value);
    } catch (const opc::OPCException& ex) {
      logger->log_warn("Failed to convert value at index {} to string, skipping entry: {}", i, ex.what());
      continue;
    }
    entry.source_timestamp = data_values[i].sourceTimestamp;
    entry.modification_info = (modification_infos && i < modification_infos_size) ? &modification_infos[i] : nullptr;
    batch.entries.push_back(std::move(entry));
  }
  return batch;
}

std::optional<FetchedState> parseFetchedState(const std::unordered_map<std::string, std::string>& state_map) {
  const auto timestamp_it = state_map.find(LAST_FETCHED_TIMESTAMP_KEY);
  const auto fingerprints_it = state_map.find(LAST_FETCHED_FINGERPRINT_KEY);
  if (timestamp_it == state_map.end() || fingerprints_it == state_map.end()) {
    return std::nullopt;
  }

  FetchedState state;
  state.timestamp = std::stoll(timestamp_it->second);
  for (auto& fingerprint : utils::string::split(fingerprints_it->second, ",")) {
    if (!fingerprint.empty()) {
      state.fingerprints.insert(std::move(fingerprint));
    }
  }
  return state;
}

std::vector<HistoryEntry> selectNewEntries(std::vector<HistoryEntry> entries, const std::optional<FetchedState>& last_fetched,
    bool has_modification_info, std::optional<size_t> max_entries) {
  std::vector<HistoryEntry> new_entries;
  new_entries.reserve(max_entries ? std::min(*max_entries, entries.size()) : entries.size());
  for (const auto& entry : entries) {
    if (max_entries && new_entries.size() >= *max_entries) {
      break;
    }
    if (last_fetched && entry.source_timestamp == last_fetched->timestamp &&
        last_fetched->fingerprints.contains(entryFingerprint(entry, has_modification_info))) {
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

core::Record toRecord(const std::string& node_id, const int32_t namespace_index, const HistoryEntry& entry) {
  core::Record record;
  record.emplace("Value", core::RecordField(std::string(entry.value)));
  record.emplace("NodeID", core::RecordField(node_id));
  record.emplace("NamespaceIndex", core::RecordField(std::to_string(namespace_index)));
  record.emplace("Sourcetimestamp", core::RecordField(opc::OPCDateTime2String(entry.source_timestamp)));
  if (entry.modification_info) {
    addModificationInfo(record, *entry.modification_info);
  }
  return record;
}

void writeAsRecordSet(FetchOPCHistoryContext& context, const std::vector<HistoryEntry>& entries) {
  core::RecordSet record_set;
  for (const auto& entry : entries) {
    record_set.push_back(toRecord(context.node_id, context.namespace_index, entry));
  }

  auto flow_file = context.session.create();
  context.record_set_writer->write(record_set, flow_file, context.session);
  context.session.transfer(flow_file, FetchOPCHistory::Success);
  context.entries_transferred += entries.size();
}

void writeAsFlowFiles(FetchOPCHistoryContext& context, const std::vector<HistoryEntry>& entries) {
  for (const auto& entry : entries) {
    auto flow_file = context.session.create();
    context.session.write(flow_file, [&entry](const std::shared_ptr<io::OutputStream>& output_stream) -> io::IoResult {
      output_stream->write(reinterpret_cast<const uint8_t*>(entry.value.data()), entry.value.size());
      return io::IoResult::from(entry.value.size());
    });
    flow_file->addAttribute("NodeID", context.node_id);
    flow_file->addAttribute("NamespaceIndex", std::to_string(context.namespace_index));
    flow_file->addAttribute("Sourcetimestamp", opc::OPCDateTime2String(entry.source_timestamp));
    if (entry.modification_info) {
      addModificationInfo(*flow_file, *entry.modification_info);
    }
    context.session.transfer(flow_file, FetchOPCHistory::Success);
    ++context.entries_transferred;
  }
}

void updateState(std::unordered_map<std::string, std::string>& state_map, const std::vector<HistoryEntry>& new_entries, bool has_modification_info) {
  const int64_t new_timestamp = new_entries.back().source_timestamp;
  const auto previous_state = parseFetchedState(state_map);

  std::unordered_set<std::string> fingerprints;
  if (previous_state && previous_state->timestamp == new_timestamp) {
    fingerprints = previous_state->fingerprints;
  }
  for (const auto& entry : new_entries) {
    if (entry.source_timestamp == new_timestamp) {
      fingerprints.insert(entryFingerprint(entry, has_modification_info));
    }
  }

  state_map[LAST_FETCHED_TIMESTAMP_KEY] = std::to_string(new_timestamp);
  state_map[LAST_FETCHED_FINGERPRINT_KEY] = utils::string::join(",", fingerprints);
}

UA_Boolean historyReadCallback(UA_Client* /*client*/, const UA_NodeId* /*node_id*/, UA_Boolean more_data_available, const UA_ExtensionObject* data,
    void* ctx) {
  auto* opc_history_context = static_cast<FetchOPCHistoryContext*>(ctx);

  auto batch = extractHistoryBatch(data, opc_history_context->logger);
  if (batch && !batch->entries.empty()) {
    const auto last_fetched = parseFetchedState(opc_history_context->state_map);
    const std::optional<size_t> remaining = opc_history_context->batch_size != 0
        ? std::optional<size_t>(opc_history_context->batch_size - opc_history_context->entries_transferred)
        : std::nullopt;
    auto new_entries = selectNewEntries(std::move(batch->entries), last_fetched, batch->has_modification_info, remaining);

    if (!new_entries.empty()) {
      if (opc_history_context->record_set_writer) {
        writeAsRecordSet(*opc_history_context, new_entries);
      } else {
        writeAsFlowFiles(*opc_history_context, new_entries);
      }
      updateState(opc_history_context->state_map, new_entries, batch->has_modification_info);
    }
  }

  const bool batch_limit_reached = opc_history_context->batch_size != 0 &&
      opc_history_context->entries_transferred >= opc_history_context->batch_size;
  return more_data_available && !batch_limit_reached;
}

UA_DateTime calculateStartTime(const std::unordered_map<std::string, std::string>& state_map,
    const std::optional<std::chrono::system_clock::time_point>& start_timestamp) {
  if (state_map.find(LAST_FETCHED_TIMESTAMP_KEY) != state_map.end()) {
    return std::stoll(state_map.at(LAST_FETCHED_TIMESTAMP_KEY).c_str());
  } else if (start_timestamp.has_value()) {
    uint64_t start_time_seconds = std::chrono::duration_cast<std::chrono::seconds>(start_timestamp->time_since_epoch()).count();
    return UA_DateTime_fromUnixTime(start_time_seconds);
  }
  return UA_DateTime_fromUnixTime(0);
}

UA_DateTime calculateEndTime(const std::optional<std::chrono::system_clock::time_point>& end_timestamp) {
  if (end_timestamp.has_value()) {
    uint64_t end_time_seconds = std::chrono::duration_cast<std::chrono::seconds>(end_timestamp->time_since_epoch()).count();
    return UA_DateTime_fromUnixTime(end_time_seconds);
  }
  return UA_DateTime_now();
}

}  // namespace

void FetchOPCHistory::initialize() {
  setSupportedProperties(Properties);
  setSupportedRelationships(Relationships);
}

void FetchOPCHistory::onSchedule(core::ProcessContext& context, core::ProcessSessionFactory& factory) {
  logger_->log_trace("FetchOPCHistory::onSchedule");
  BaseOPCProcessor::onSchedule(context, factory);
  node_id_ = utils::parseProperty(context, NodeID);
  parseIdType(context, NodeIDType);
  namespace_idx_ = gsl::narrow<int32_t>(utils::parseI64Property(context, NameSpaceIndex));

  switch (id_type_) {
    case opc::OPCNodeIDType::String:
      node_ = UA_NODEID_STRING(namespace_idx_, const_cast<char*>(node_id_.c_str()));
      break;
    case opc::OPCNodeIDType::Int:
      node_ = UA_NODEID_NUMERIC(namespace_idx_, std::stoi(node_id_));
      break;
    case opc::OPCNodeIDType::Guid:
      node_ = UA_NODEID_GUID(namespace_idx_, UA_GUID(node_id_.c_str()));
      break;
    default:
      throw Exception(PROCESS_SCHEDULE_EXCEPTION, fmt::format("Unsupported Node ID type: {}", magic_enum::enum_name(id_type_)));
  }

  history_type_ = utils::parseEnumProperty<opc::HistoryReadTypeOption>(context, HistoryReadType);
  start_timestamp_ = utils::parseOptionalProperty(context, StartTimestamp) | utils::andThen(utils::timeutils::parseDateTimeStr);
  end_timestamp_ = utils::parseOptionalProperty(context, EndTimestamp) | utils::andThen(utils::timeutils::parseDateTimeStr);
  batch_size_ = utils::parseOptionalU64Property(context, BatchSize).value_or(0);
  const auto record_set_writer_name = context.getProperty(RecordSetWriter).value_or("");
  auto controller_service = context.getControllerService(record_set_writer_name, getUUID());
  if (!record_set_writer_name.empty() && !controller_service) {
    throw Exception(PROCESS_SCHEDULE_EXCEPTION, fmt::format("Controller service '{}' not found", record_set_writer_name));
  }
  record_set_writer_ = std::dynamic_pointer_cast<core::RecordSetWriter>(controller_service);
}

void FetchOPCHistory::onTrigger(core::ProcessContext& context, core::ProcessSession& session) {
  logger_->log_trace("FetchOPCHistory::onTrigger");

  if (!reconnect()) {
    context.yield();
    return;
  }

  auto* state_manager = context.getStateManager();
  std::unordered_map<std::string, std::string> state_map;

  state_manager->get(state_map);

  size_t entries_transferred = 0;
  FetchOPCHistoryContext history_context{session, record_set_writer_, state_map, entries_transferred, batch_size_, node_id_, namespace_idx_, logger_};

  auto retval = connection_->readHistory(history_type_,
      node_,
      &historyReadCallback,
      calculateStartTime(state_map, start_timestamp_),
      calculateEndTime(end_timestamp_),
      static_cast<void*>(&history_context));

  if (retval != UA_STATUSCODE_GOOD) {
    logger_->log_error("Failed to read OPC UA node history, status code: {:#010x}", retval);
  }

  state_manager->set(state_map);
}

REGISTER_RESOURCE(FetchOPCHistory, Processor);

}  // namespace org::apache::nifi::minifi::processors
