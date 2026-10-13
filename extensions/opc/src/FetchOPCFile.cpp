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
#include "FetchOPCFile.h"

#include <algorithm>
#include <limits>
#include <memory>
#include <string>
#include <vector>

#include "core/Resource.h"
#include "minifi-cpp/core/ProcessContext.h"
#include "utils/ProcessorConfigUtils.h"

namespace org::apache::nifi::minifi::processors {

void FetchOPCFile::initialize() {
  setSupportedProperties(Properties);
  setSupportedRelationships(Relationships);
}

void FetchOPCFile::onSchedule(core::ProcessContext& context, core::ProcessSessionFactory& factory) {
  logger_->log_trace("FetchOPCFile::onSchedule");

  BaseOPCProcessor::onSchedule(context, factory);

  node_id_ = utils::parseProperty(context, NodeID);
  id_type_ = utils::parseEnumProperty<opc::OPCNodeIDType>(context, NodeIDType);
  namespace_idx_ = gsl::narrow<UA_UInt16>(utils::parseU64Property(context, NameSpaceIndex));
  parseNode(context);

  chunk_size_ = utils::parseDataSizeProperty(context, ChunkSize);
  if (chunk_size_ == 0 || chunk_size_ > static_cast<uint64_t>(std::numeric_limits<UA_Int32>::max())) {
    throw Exception(PROCESS_SCHEDULE_EXCEPTION, fmt::format("Chunk size must be between 1 and {} bytes", std::numeric_limits<UA_Int32>::max()));
  }
}

bool FetchOPCFile::resolvePath() {
  if (id_type_ != opc::OPCNodeIDType::Path || path_node_id_resolved_) {
    return true;
  }
  std::vector<opc::NodeId> translated_node_ids;
  auto sc = connection_->translateBrowsePathsToNodeIdsRequest(node_id_, translated_node_ids, namespace_idx_, path_reference_types_, logger_);
  if (sc != UA_STATUSCODE_GOOD) {
    logger_->log_error("Failed to translate path '{}' to a node id: {}", node_id_, UA_StatusCode_name(sc));
    return false;
  }
  if (translated_node_ids.size() != 1) {
    logger_->log_error("Path '{}' resolved to {} node ids; exactly one file node is required for fetch", node_id_, translated_node_ids.size());
    return false;
  }
  node_ = std::move(translated_node_ids[0]);
  path_node_id_resolved_ = true;
  return true;
}

bool FetchOPCFile::writeOPCFileToFlowFile(opc::RemoteFile& remote_file, UA_UInt32 handle, core::ProcessSession& session, const std::shared_ptr<core::FlowFile>& flow_file) {
  opc::RemoteFileHandle file_handle(remote_file, handle);

  const auto chunk_size = gsl::narrow<UA_Int32>(std::min<uint64_t>(chunk_size_,
      remote_file.maxByteStringLength().value_or(std::numeric_limits<UA_UInt32>::max())));
  UA_StatusCode read_status = UA_STATUSCODE_GOOD;
  session.write(flow_file, [&](const std::shared_ptr<io::OutputStream>& output_stream) -> io::IoResult {
    size_t total_size = 0;
    while (true) {
      auto chunk = remote_file.read(file_handle.get(), chunk_size);
      if (!chunk) {
        read_status = chunk.error();
        return io::IoResult::cancelled();
      }
      if (chunk->empty()) {
        return io::IoResult::from(total_size);
      }
      const auto written = output_stream->write(std::span<const std::byte>(*chunk));
      if (io::isError(written)) {
        return io::IoResult::error();
      }
      total_size += chunk->size();
    }
  });

  if (read_status != UA_STATUSCODE_GOOD) {
    logger_->log_error("Failed to read file node '{}': {}", node_id_, UA_StatusCode_name(read_status));
    session.remove(flow_file);
    return false;
  }

  if (auto sc = file_handle.close(); sc != UA_STATUSCODE_GOOD) {
    logger_->log_warn("Failed to close file node '{}' after reading it: {}", node_id_, UA_StatusCode_name(sc));
  }
  return true;
}

void FetchOPCFile::onTrigger(core::ProcessContext& context, core::ProcessSession& session) {
  logger_->log_trace("FetchOPCFile::onTrigger");

  if (!reconnect()) {
    logger_->log_warn("Could not connect to OPC server, yielding");
    context.yield();
    return;
  }

  if (!resolvePath()) {
    context.yield();
    return;
  }

  auto remote_file = opc::RemoteFile::create(*connection_, opc::NodeId::copyOf(node_));
  if (!remote_file) {
    logger_->log_error("Node '{}' is not a file node, failed to find its file methods: {}", node_id_, UA_StatusCode_name(remote_file.error()));
    context.yield();
    return;
  }

  auto handle = remote_file->open(opc::FILE_MODE_READ);
  if (!handle) {
    logger_->log_error("Failed to open file node '{}' for reading: {}", node_id_, UA_StatusCode_name(handle.error()));
    context.yield();
    return;
  }

  auto flow_file = session.create();
  if (!writeOPCFileToFlowFile(*remote_file, *handle, session, flow_file)) {
    context.yield();
    return;
  }

  for (const auto& [key, value] : opc::nodeIdAttributes(node_)) {
    flow_file->setAttribute(key, value);
  }
  if (auto browse_name = connection_->readBrowseName(node_)) {
    flow_file->setAttribute(BrowsenameAttr.name, *browse_name);
  }
  session.transfer(flow_file, Success);
}

REGISTER_RESOURCE(FetchOPCFile, Processor);

}  // namespace org::apache::nifi::minifi::processors
