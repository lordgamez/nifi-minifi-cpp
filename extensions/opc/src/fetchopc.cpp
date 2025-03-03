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

#include <memory>
#include <string>
#include <list>

#include "opc.h"
#include "fetchopc.h"
#include "core/Processor.h"
#include "core/ProcessSession.h"
#include "core/Resource.h"
#include "utils/StringUtils.h"
#include "utils/Enum.h"
#include "core/ProcessContext.h"

namespace org::apache::nifi::minifi::processors {

void FetchOPCProcessor::initialize() {
  setSupportedProperties(Properties);
  setSupportedRelationships(Relationships);
}

void FetchOPCProcessor::onSchedule(core::ProcessContext& context, core::ProcessSessionFactory& factory) {
  logger_->log_trace("FetchOPCProcessor::onSchedule");

  translated_node_ids_.clear();  // Path might has changed during restart

  BaseOPCProcessor::onSchedule(context, factory);

  std::string value;
  context.getProperty(NodeID, nodeID_);
  context.getProperty(NodeIDType, value);

  max_depth_ = 0;
  context.getProperty(MaxDepth, max_depth_);

  if (value == "String") {
    id_type_ = opc::OPCNodeIDType::String;
  } else if (value == "Int") {
    id_type_ = opc::OPCNodeIDType::Int;
  } else if (value == "Path") {
    id_type_ = opc::OPCNodeIDType::Path;
  } else {
    // Where have our validators gone?
    auto error_msg = utils::string::join_pack(value, " is not a valid node ID type!");
    throw Exception(PROCESS_SCHEDULE_EXCEPTION, error_msg);
  }

  if (id_type_ == opc::OPCNodeIDType::Int) {
    try {
      // ensure that nodeID_ can be parsed as an int
      static_cast<void>(std::stoi(nodeID_));
    } catch(...) {
      auto error_msg = utils::string::join_pack(nodeID_, " cannot be used as an int type node ID");
      throw Exception(PROCESS_SCHEDULE_EXCEPTION, error_msg);
    }
  }
  if (!context.getProperty(NameSpaceIndex, namespace_idx_)) {
    auto error_msg = utils::string::join_pack(NameSpaceIndex.name, " is mandatory");
    throw Exception(PROCESS_SCHEDULE_EXCEPTION, error_msg);
  }

  context.getProperty(Lazy, value);
  lazy_mode_ = value == "On";

  if (id_type_ == opc::OPCNodeIDType::Path) {
    readPathReferenceTypes(context, nodeID_);
  }
}

void FetchOPCProcessor::onTrigger(core::ProcessContext& context, core::ProcessSession& session) {
  logger_->log_trace("FetchOPCProcessor::onTrigger");

  if (!reconnect()) {
    yield();
    return;
  }

  size_t nodes_found = 0;
  size_t variables_found = 0;

  auto found_cb = [this, &context, &session, &nodes_found, &variables_found](const UA_ReferenceDescription* ref, const std::string& path) {
    return nodeFoundCallBack(ref, path, context, session, nodes_found, variables_found); };
  if (id_type_ != opc::OPCNodeIDType::Path) {
    UA_NodeId myID;
    myID.namespaceIndex = namespace_idx_;
    if (id_type_ == opc::OPCNodeIDType::Int) {
      myID.identifierType = UA_NODEIDTYPE_NUMERIC;
      myID.identifier.numeric = std::stoi(nodeID_);  // NOLINT(cppcoreguidelines-pro-type-union-access)
    } else if (id_type_ == opc::OPCNodeIDType::String) {
      myID.identifierType = UA_NODEIDTYPE_STRING;
      myID.identifier.string = UA_STRING_ALLOC(nodeID_.c_str());  // NOLINT(cppcoreguidelines-pro-type-union-access)
    } else {
      logger_->log_error("Unhandled id type: '{}'. No flowfiles are generated.", magic_enum::enum_underlying(id_type_));
      yield();
      return;
    }
    connection_->traverse(myID, found_cb, "", max_depth_);
  } else {
    if (translated_node_ids_.empty()) {
      auto sc = connection_->translateBrowsePathsToNodeIdsRequest(nodeID_, translated_node_ids_, namespace_idx_, pathReferenceTypes_, logger_);
      if (sc != UA_STATUSCODE_GOOD) {
        logger_->log_error("Failed to translate {} to node id, no flow files will be generated ({})", nodeID_.c_str(), UA_StatusCode_name(sc));
        yield();
        return;
      }
    }
    for (auto& nodeID : translated_node_ids_) {
      connection_->traverse(nodeID, found_cb, nodeID_, max_depth_);
    }
  }
  if (nodes_found == 0) {
    logger_->log_warn("Connected to OPC server, but no variable nodes were found. Configuration might be incorrect! Yielding...");
    yield();
  } else if (variables_found == 0) {
    logger_->log_warn("Found no variables when traversing the specified node. No flowfiles are generated. Yielding...");
    yield();
  }
}

bool FetchOPCProcessor::nodeFoundCallBack(const UA_ReferenceDescription *ref, const std::string& path,
    core::ProcessContext& context, core::ProcessSession& session, size_t& nodes_found, size_t& variables_found) {
  ++nodes_found;
  if (ref->nodeClass == UA_NODECLASS_VARIABLE) {
    try {
      opc::NodeData nodedata = connection_->getNodeData(ref, path);
      bool write = true;
      if (lazy_mode_) {
        write = false;
        std::string nodeid = nodedata.attributes["Full path"];
        std::string cur_timestamp = node_timestamp_[nodeid];
        std::string new_timestamp = nodedata.attributes["Sourcetimestamp"];
        if (cur_timestamp != new_timestamp) {
          node_timestamp_[nodeid] = new_timestamp;
          logger_->log_debug("Node {} has new source timestamp {}", nodeid, new_timestamp);
          write = true;
        }
      }
      if (write) {
        OPCData2FlowFile(nodedata, context, session);
        ++variables_found;
      }
    } catch (const std::exception& exception) {
      std::string browse_name(reinterpret_cast<char*>(ref->browseName.name.data), ref->browseName.name.length);
      logger_->log_warn("Caught Exception while trying to get data from node {}: {}", path + "/" + browse_name, exception.what());
    }
  }
  return true;
}

void FetchOPCProcessor::OPCData2FlowFile(const opc::NodeData& opcnode, core::ProcessContext&, core::ProcessSession& session) {
  auto flowFile = session.create();
  if (flowFile == nullptr) {
    logger_->log_error("Failed to create flowfile!");
    return;
  }
  for (const auto& attr : opcnode.attributes) {
    flowFile->setAttribute(attr.first, attr.second);
  }
  if (!opcnode.data.empty()) {
    try {
      session.writeBuffer(flowFile, opc::nodeValue2String(opcnode));
    } catch (const std::exception& e) {
      std::string browsename;
      flowFile->getAttribute("Browsename", browsename);
      logger_->log_info("Failed to extract data of OPC node {}: {}", browsename, e.what());
      session.transfer(flowFile, Failure);
      return;
    }
  }
  session.transfer(flowFile, Success);
}

REGISTER_RESOURCE(FetchOPCProcessor, Processor);

}  // namespace org::apache::nifi::minifi::processors
