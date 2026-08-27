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

namespace org::apache::nifi::minifi::processors {

namespace {

std::string formatUADateTime(UA_DateTime dt) {
  UA_DateTimeStruct dts = UA_DateTime_toStruct(dt);
  char buf[32];
  snprintf(buf, sizeof(buf), "%04u-%02u-%02uT%02u:%02u:%02u.%03uZ",
           dts.year, dts.month, dts.day, dts.hour, dts.min, dts.sec, dts.milliSec);
  return buf;
}

const char *updateTypeToString(UA_HistoryUpdateType type) {
  switch (type) {
    case UA_HISTORYUPDATETYPE_INSERT:  return "Insert";
    case UA_HISTORYUPDATETYPE_REPLACE: return "Replace";
    case UA_HISTORYUPDATETYPE_UPDATE:  return "Update";
    case UA_HISTORYUPDATETYPE_DELETE:  return "Delete";
    default:                            return "Unknown";
  }
}

}  // namespace

void FetchOpcHistory::initialize() {
  setSupportedProperties(Properties);
  setSupportedRelationships(Relationships);
}

UA_Boolean FetchOpcHistory::historyReadCallback(UA_Client * /*client*/,
                                                 const UA_NodeId * /*nodeId*/,
                                                 UA_Boolean moreDataAvailable,
                                                 const UA_ExtensionObject *data,
                                                 void *ctx) {

  const UA_DataValue *dataValues = nullptr;
  size_t dataValuesSize = 0;
  const UA_ModificationInfo *modificationInfos = nullptr;
  size_t modificationInfosSize = 0;

  if (data->content.decoded.type == &UA_TYPES[UA_TYPES_HISTORYMODIFIEDDATA]) {
    const auto *modifiedData =
        static_cast<const UA_HistoryModifiedData *>(data->content.decoded.data);
    dataValues = modifiedData->dataValues;
    dataValuesSize = modifiedData->dataValuesSize;
    modificationInfos = modifiedData->modificationInfos;
    modificationInfosSize = modifiedData->modificationInfosSize;
  } else {
    // Unexpected data type received in the callback
    return false;
  }

  for (size_t i = 0; i < dataValuesSize; ++i) {

    const UA_DataValue &value = dataValues[i];
    // modificationInfos is parallel to dataValues by index per the OPC UA
    // spec (Part 11): entry i describes value i, or is "empty" (Insert with
    // no prior value / null username) if the value was never edited.
    const UA_ModificationInfo *mod_info = (modificationInfos && i < modificationInfosSize) ? &modificationInfos[i] : nullptr;

    NodeModificationData node_mod_data;
    if (value.value.type == &UA_TYPES[UA_TYPES_DOUBLE]) {
      node_mod_data.value = std::to_string(*static_cast<UA_Double *>(value.value.data));
    } else if (value.value.type == &UA_TYPES[UA_TYPES_UINT32]) {
      node_mod_data.value = std::to_string(*static_cast<UA_UInt32 *>(value.value.data));
    }

    if (mod_info) {
      if (mod_info->userName.length > 0) {
        node_mod_data.username = std::string(reinterpret_cast<char *>(mod_info->userName.data), mod_info->userName.length);
      }
      node_mod_data.modification_time = formatUADateTime(mod_info->modificationTime);
      node_mod_data.updateType = updateTypeToString(mod_info->updateType);
    }

    FetchOpcHistoryContext *context = static_cast<FetchOpcHistoryContext*>(ctx);
    auto flow_file = context->session.create();
    context->session.write(flow_file, [&](const std::shared_ptr<io::OutputStream>& output_stream) -> io::IoResult {
      output_stream->write(reinterpret_cast<const uint8_t*>(node_mod_data.value.data()), node_mod_data.value.size());
      return io::IoResult::from(node_mod_data.value.size());
    });

    flow_file->addAttribute("ModificationUsername", node_mod_data.username);
    flow_file->addAttribute("ModificationTime", node_mod_data.modification_time);
    flow_file->addAttribute("ModificationUpdateType", node_mod_data.updateType);
    context->session.transfer(flow_file, Success);
  }


  return true;
}


void FetchOpcHistory::onSchedule(core::ProcessContext& context, core::ProcessSessionFactory& factory) {
  logger_->log_trace("FetchOpcHistory::onSchedule");

  BaseOPCProcessor::onSchedule(context, factory);

  node_id_ = utils::parseProperty(context, NodeID);

  parseIdType(context, NodeIDType);

  namespace_idx_ = gsl::narrow<int32_t>(utils::parseI64Property(context, NameSpaceIndex));

}

void FetchOpcHistory::onTrigger(core::ProcessContext& context, core::ProcessSession& session) {
  logger_->log_trace("FetchOpcHistory::onTrigger");

  if (!reconnect()) {
    context.yield();
    return;
  }

  auto* state_manager = context.getStateManager();
  std::unordered_map<std::string, std::string> state_map;
  state_manager->get(state_map);

  UA_NodeId node = UA_NODEID_STRING(namespace_idx_, const_cast<char*>(node_id_.c_str()));

  FetchOpcHistoryContext ctx{session};
  auto retval = connection_->readHistory(node, &FetchOpcHistory::historyReadCallback, UA_DateTime_fromUnixTime(0), UA_DateTime_now(), 10, (void *)&ctx);

  if (retval != UA_STATUSCODE_GOOD) {
    throw std::runtime_error(fmt::format("Failed to read history for node {}: {}", node_id_, UA_StatusCode_name(retval)));
  }

  state_manager->set(state_map);
}


REGISTER_RESOURCE(FetchOpcHistory, Processor);

}  // namespace org::apache::nifi::minifi::processors
