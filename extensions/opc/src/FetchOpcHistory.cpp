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

UA_Boolean FetchOpcHistory::historyReadCallback(UA_Client* /*client*/, const UA_NodeId* /*node_id*/, UA_Boolean /*more_data_available*/, const UA_ExtensionObject* data, void* ctx) {
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
    return true;  // No data to process
  }

  auto* context = static_cast<FetchOpcHistoryContext*>(ctx);
  if (context->record_set_writer) {
    core::RecordSet record_set;
    auto flow_file = context->session.create();

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

      core::Record record;
      record.emplace("Value", core::RecordField(std::move(node_mod_data_value)));
      if (mod_info) {
        if (mod_info->userName.length > 0) {
          record.emplace("ModificationUsername", core::RecordField(std::string(reinterpret_cast<char *>(mod_info->userName.data), mod_info->userName.length)));
        }
        record.emplace("ModificationTime", core::RecordField(opc::OPCDateTime2String(mod_info->modificationTime)));
        record.emplace("ModificationUpdateType", core::RecordField(updateTypeToString(mod_info->updateType)));
      }
      record_set.push_back(std::move(record));
    }

    context->record_set_writer->write(record_set, flow_file, context->session);
    context->session.transfer(flow_file, Success);
  } else {

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


      auto flow_file = context->session.create();
      context->session.write(flow_file, [&](const std::shared_ptr<io::OutputStream>& output_stream) -> io::IoResult {
        output_stream->write(reinterpret_cast<const uint8_t*>(node_mod_data_value.data()), node_mod_data_value.size());
        return io::IoResult::from(node_mod_data_value.size());
      });

      if (mod_info) {
        if (mod_info->userName.length > 0) {
          flow_file->addAttribute("ModificationUsername", std::string(reinterpret_cast<char *>(mod_info->userName.data), mod_info->userName.length));
        }

        flow_file->addAttribute("ModificationTime", opc::OPCDateTime2String(mod_info->modificationTime));
        flow_file->addAttribute("ModificationUpdateType", updateTypeToString(mod_info->updateType));
      }

      context->session.transfer(flow_file, Success);
    }
  }

  return true;
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

  FetchOpcHistoryContext ctx{session, record_set_writer_};

  auto retval = connection_->readHistory(history_type_, node, &FetchOpcHistory::historyReadCallback, start_timestamp_, end_timestamp_, batch_size_, (void *)&ctx);

  if (retval != UA_STATUSCODE_GOOD) {
    // TODO: handle error, possibly yield and log the error
  }

  state_manager->set(state_map);
}

REGISTER_RESOURCE(FetchOpcHistory, Processor);

}  // namespace org::apache::nifi::minifi::processors
