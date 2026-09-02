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
#include <string>

#include "minifi-cpp/core/ProcessContext.h"
#include "core/ProcessSession.h"
#include "core/Resource.h"
#include "utils/ProcessorConfigUtils.h"
#include "utils/TimeUtil.h"
#include "rapidjson/rapidjson.h"
#include "rapidjson/document.h"
#include "rapidjson/stringbuffer.h"
#include "rapidjson/writer.h"
#include "utils/StringUtils.h"

namespace org::apache::nifi::minifi::processors {

namespace {

std::string updateTypeToString(UA_HistoryUpdateType type) {
  switch (type) {
    case UA_HISTORYUPDATETYPE_INSERT: return "Insert";
    case UA_HISTORYUPDATETYPE_REPLACE: return "Replace";
    case UA_HISTORYUPDATETYPE_UPDATE: return "Update";
    case UA_HISTORYUPDATETYPE_DELETE: return "Delete";
    default: return "Unknown";
  }
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
  const auto record_set_writer_name = context.getProperty(RecordSetWriter).value_or("");;
  record_set_writer_ = std::dynamic_pointer_cast<core::RecordSetWriter>(context.getControllerService(record_set_writer_name, getUUID()));
}

UA_Boolean FetchOpcHistory::historyReadCallback(UA_Client* /*client*/, const UA_NodeId* /*node_id*/, UA_Boolean more_data_available, const UA_ExtensionObject* data, void* ctx) {
  const UA_DataValue* data_values = nullptr;
  size_t data_value_size = 0;
  const UA_ModificationInfo* modification_infos = nullptr;
  size_t modification_infos_size = 0;

  if (data->content.decoded.type == &UA_TYPES[UA_TYPES_HISTORYDATA]) {
    const auto *historyData = static_cast<const UA_HistoryData *>(data->content.decoded.data);
    data_values = historyData->dataValues;
    data_value_size = historyData->dataValuesSize;
  } else if (data->content.decoded.type == &UA_TYPES[UA_TYPES_HISTORYMODIFIEDDATA]) {
    const auto *modifiedData = static_cast<const UA_HistoryModifiedData *>(data->content.decoded.data);
    data_values = modifiedData->dataValues;
    data_value_size = modifiedData->dataValuesSize;
    modification_infos = modifiedData->modificationInfos;
    modification_infos_size = modifiedData->modificationInfosSize;
  } else {
    // TODO: Unexpected data type received in the callback, how to handle this?
    return false;
  }

  if (data_value_size == 0) {
    return false;  // No data to process
  }

  auto* context = static_cast<FetchOpcHistoryContext*>(ctx);
  context->has_more_data = more_data_available;

  std::optional<int64_t> last_fetched_source_timestamp;
  std::optional<int64_t> last_fetched_modification_timestamp;
  std::optional<std::string> last_fetched_value;
  if (context->state_map.find("last_fetched_fingerprint") != context->state_map.end()) {
    auto splitted_state = utils::string::split(context->state_map["last_fetched_fingerprint"], ":");
    if (modification_infos) {
      if (splitted_state.size() == 3) {
        last_fetched_source_timestamp = std::stoll(splitted_state[0]);
        last_fetched_modification_timestamp = std::stoll(splitted_state[1]);
        last_fetched_value = splitted_state[2];
      } else {
        // TODO: warning
      }
    } else {
      if (splitted_state.size() == 2) {
        last_fetched_source_timestamp = std::stoll(splitted_state[0]);
        last_fetched_value = splitted_state[1];
      } else {
        // TODO: warning
      }
    }
  }

  std::string new_last_fetched_value;
  int64_t new_last_fetched_source_timestamp = 0;
  int64_t new_last_fetched_modification_timestamp = 0;
  if (context->record_set_writer) {
    core::RecordSet record_set;
    bool already_fetched_found = false;
    for (size_t i = 0; i < data_value_size; ++i) {
      const UA_DataValue &value = data_values[i];
      // modificationInfos is parallel to dataValues by index (OPC UA Part 11):
      // entry i describes value i, or is absent if the value was never edited.
      const UA_ModificationInfo* mod_info = (modification_infos && i < modification_infos_size) ? &modification_infos[i] : nullptr;

      std::string node_mod_data_value;
      try {
        node_mod_data_value = opc::variantToString(value.value);
      } catch (const opc::OPCException&) {
        // Unsupported value type: leave content empty. An exception must not unwind across the C history-read callback boundary in open62541.
        // TODO: Log a warning about the unsupported value type.
      }

      auto source_timestamp = value.sourceTimestamp;
      auto modification_timestamp = mod_info ? mod_info->modificationTime : UA_DateTime_fromUnixTime(0);
      if (last_fetched_source_timestamp && last_fetched_value) {
        if (source_timestamp == *last_fetched_source_timestamp) {
          if (last_fetched_modification_timestamp) {
            if (!already_fetched_found) {
              if (modification_timestamp == *last_fetched_modification_timestamp && node_mod_data_value == *last_fetched_value) {
                already_fetched_found = true;
              }
              continue;
            }
          } else {
            if (!already_fetched_found) {
              if (node_mod_data_value == *last_fetched_value) {
                already_fetched_found = true;
              }
              continue;
            }
          }
        }
      }

      core::Record record;
      new_last_fetched_source_timestamp = source_timestamp;
      new_last_fetched_value = node_mod_data_value;
      record.emplace("Value", core::RecordField(std::move(node_mod_data_value)));
      if (mod_info) {
        if (mod_info->userName.length > 0) {
          record.emplace("ModificationUsername", core::RecordField(std::string(reinterpret_cast<char *>(mod_info->userName.data), mod_info->userName.length)));
        }
        new_last_fetched_modification_timestamp = mod_info->modificationTime;
        record.emplace("ModificationTime", core::RecordField(opc::OPCDateTime2String(mod_info->modificationTime)));
        record.emplace("ModificationUpdateType", core::RecordField(updateTypeToString(mod_info->updateType)));
      }
      record_set.push_back(std::move(record));
    }

    if (record_set.empty()) {
      return false;
    }

    auto flow_file = context->session.create();
    context->record_set_writer->write(record_set, flow_file, context->session);
    context->session.transfer(flow_file, Success);
    ++context->flow_files_transferred;
  } else {
    bool already_fetched_found = false;
    for (size_t i = 0; i < data_value_size; ++i) {
      const UA_DataValue &value = data_values[i];
      // modificationInfos is parallel to dataValues by index (OPC UA Part 11):
      // entry i describes value i, or is absent if the value was never edited.
      const UA_ModificationInfo* mod_info = (modification_infos && i < modification_infos_size) ? &modification_infos[i] : nullptr;

      std::string node_mod_data_value;
      try {
        node_mod_data_value = opc::variantToString(value.value);
      } catch (const opc::OPCException&) {
        // Unsupported value type: leave content empty. An exception must not unwind across the C history-read callback boundary in open62541.
        // TODO: Log a warning about the unsupported value type.
      }

      auto source_timestamp = value.sourceTimestamp;
      auto modification_timestamp = mod_info ? mod_info->modificationTime : UA_DateTime_fromUnixTime(0);
      if (last_fetched_source_timestamp && last_fetched_value) {
        if (source_timestamp == *last_fetched_source_timestamp) {
          if (last_fetched_modification_timestamp) {
            if (!already_fetched_found) {
              if (modification_timestamp == *last_fetched_modification_timestamp && node_mod_data_value == *last_fetched_value) {
                already_fetched_found = true;
              }
              continue;
            }
          } else {
            if (!already_fetched_found) {
              if (node_mod_data_value == *last_fetched_value) {
                already_fetched_found = true;
              }
              continue;
            }
          }
        }
      }

      new_last_fetched_source_timestamp = source_timestamp;
      new_last_fetched_value = node_mod_data_value;
      auto flow_file = context->session.create();
      context->session.write(flow_file, [&](const std::shared_ptr<io::OutputStream>& output_stream) -> io::IoResult {
        output_stream->write(reinterpret_cast<const uint8_t*>(node_mod_data_value.data()), node_mod_data_value.size());
        return io::IoResult::from(node_mod_data_value.size());
      });

      if (mod_info) {
        if (mod_info->userName.length > 0) {
          flow_file->addAttribute("ModificationUsername", std::string(reinterpret_cast<char *>(mod_info->userName.data), mod_info->userName.length));
        }

        new_last_fetched_modification_timestamp = mod_info->modificationTime;
        flow_file->addAttribute("ModificationTime", opc::OPCDateTime2String(mod_info->modificationTime));
        flow_file->addAttribute("ModificationUpdateType", updateTypeToString(mod_info->updateType));
      }

      context->session.transfer(flow_file, Success);
      ++context->flow_files_transferred;
    }
  }

  if (new_last_fetched_source_timestamp > 0) {
    std::string new_fingerprint = std::to_string(new_last_fetched_source_timestamp) + ":";
    if (new_last_fetched_modification_timestamp > 0) {
      new_fingerprint += std::to_string(new_last_fetched_modification_timestamp) + ":";
    }
    new_fingerprint += new_last_fetched_value;
    context->state_map["last_fetched_timestamp"] = std::to_string(new_last_fetched_source_timestamp);
    context->state_map["last_fetched_fingerprint"] = new_fingerprint;
  }

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

  // TODO: handle previous state to avoid fetching the same history entries multiple times. This may require storing the last fetched timestamp or entry ID in the state manager.
  state_manager->get(state_map);

  UA_NodeId node = UA_NODEID_STRING(namespace_idx_, const_cast<char*>(node_id_.c_str()));
  bool has_more_data = true;
  size_t flow_files_transferred = 0;
  FetchOpcHistoryContext ctx{session, record_set_writer_, state_map, has_more_data, flow_files_transferred};

  UA_DateTime ua_start_time = UA_DateTime_fromUnixTime(0);
  UA_DateTime ua_end_time = UA_DateTime_now();
  if (state_map.find("last_fetched_timestamp") != state_map.end()) {
    ua_start_time = std::stoll(state_map["last_fetched_timestamp"].c_str());
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
    auto retval = connection_->readHistory(history_type_, node, &FetchOpcHistory::historyReadCallback, ua_start_time, ua_end_time, number_of_entries_to_fetch, (void *)&ctx);

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
