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
#include "PutOPCFile.h"

#include <algorithm>
#include <limits>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "OPCFileTransfer.h"
#include "core/Resource.h"
#include "minifi-cpp/core/ProcessContext.h"
#include "utils/ProcessorConfigUtils.h"
#include "utils/StringUtils.h"

namespace org::apache::nifi::minifi::processors {

void PutOPCFile::initialize() {
  setSupportedProperties(Properties);
  setSupportedRelationships(Relationships);
}

void PutOPCFile::onSchedule(core::ProcessContext& context, core::ProcessSessionFactory& factory) {
  logger_->log_trace("PutOPCFile::onSchedule");

  BaseOPCProcessor::onSchedule(context, factory);

  target_node_id_defined_ = context.hasNonEmptyProperty(TargetNodeID.name);
  parent_node_defined_ = false;
  if (const auto parent_node_id = utils::parseOptionalProperty(context, ParentNodeID); parent_node_id && !parent_node_id->empty()) {
    node_id_ = *parent_node_id;
    id_type_ = utils::parseEnumProperty<opc::OPCNodeIDType>(context, ParentNodeIDType);
    namespace_idx_ = gsl::narrow<UA_UInt16>(utils::parseOptionalU64Property(context, ParentNameSpaceIndex).value_or(0));
    parseNode(context);
    parent_node_defined_ = true;
  }
  if (!target_node_id_defined_ && !parent_node_defined_) {
    throw Exception(PROCESS_SCHEDULE_EXCEPTION, "Either Target node ID or Parent node ID must be set");
  }

  write_mode_ = utils::parseEnumProperty<OPCFileWriteMode>(context, WriteMode);
  chunk_size_ = utils::parseDataSizeProperty(context, ChunkSize);
  if (chunk_size_ == 0 || chunk_size_ > static_cast<uint64_t>(std::numeric_limits<UA_Int32>::max())) {
    throw Exception(PROCESS_SCHEDULE_EXCEPTION, fmt::format("Chunk size must be between 1 and {} bytes", std::numeric_limits<UA_Int32>::max()));
  }
}

bool PutOPCFile::resolveParentPath() {
  if (!parent_node_defined_ || id_type_ != opc::OPCNodeIDType::Path || path_node_id_resolved_) {
    return true;
  }
  std::vector<opc::NodeId> translated_node_ids;
  auto sc = connection_->translateBrowsePathsToNodeIdsRequest(node_id_, translated_node_ids, namespace_idx_, path_reference_types_, logger_);
  if (sc != UA_STATUSCODE_GOOD) {
    logger_->log_error("Failed to translate path '{}' to a node id: {}", node_id_, UA_StatusCode_name(sc));
    return false;
  }
  if (translated_node_ids.size() != 1) {
    logger_->log_error("Path '{}' resolved to {} node ids; exactly one parent node is required", node_id_, translated_node_ids.size());
    return false;
  }
  node_ = std::move(translated_node_ids[0]);
  path_node_id_resolved_ = true;
  return true;
}

std::expected<std::optional<opc::NodeId>, std::string> PutOPCFile::findTargetFileNodeById(core::ProcessContext& context,
    const core::FlowFile& flow_file) const {
  const auto namespace_idx_str = context.getProperty(TargetNodeNameSpaceIndex, &flow_file).value_or("0");
  UA_UInt16 namespace_idx = 0;
  try {
    namespace_idx = gsl::narrow<UA_UInt16>(std::stoul(namespace_idx_str));
  } catch (const std::exception&) {
    return std::unexpected{fmt::format("invalid target namespace index '{}'", namespace_idx_str)};
  }

  const auto target_id_type_str = context.getProperty(TargetNodeIDType, &flow_file).value_or("");
  const auto target_id_type = magic_enum::enum_cast<opc::OPCNodeIDType>(target_id_type_str);
  if (!target_id_type) {
    return std::unexpected{fmt::format("invalid target node ID type '{}'", target_id_type_str)};
  }

  const auto target_id = context.getProperty(TargetNodeID, &flow_file).value_or("");
  if (target_id.empty()) {
    return std::unexpected{"empty target node ID"};
  }

  // A file cannot be created with a requested node ID, so a path has to point to an existing file
  if (*target_id_type == opc::OPCNodeIDType::Path) {
    return resolveTargetPath(context, flow_file, target_id, namespace_idx);
  }

  auto target_node = opc::buildNodeId(*target_id_type, namespace_idx, target_id);
  if (!target_node) {
    return std::unexpected{target_node.error()};
  }
  if (!connection_->exists(*target_node)) {
    logger_->log_debug("Target file node '{}' does not exist", target_id);
    return std::nullopt;
  }
  return std::move(*target_node);
}

std::expected<opc::NodeId, std::string> PutOPCFile::resolveTargetPath(core::ProcessContext& context, const core::FlowFile& flow_file,
    const std::string& target_path, UA_UInt16 namespace_idx) const {
  std::vector<UA_UInt32> reference_types;
  const auto reference_types_str = context.getProperty(TargetPathReferenceTypes, &flow_file).value_or("");
  if (!reference_types_str.empty()) {
    const auto reference_type_names = utils::string::splitAndTrimRemovingEmpty(reference_types_str, "/");
    if (reference_type_names.size() + 1 != utils::string::splitAndTrimRemovingEmpty(target_path, "/").size()) {
      return std::unexpected{fmt::format("target path reference types '{}' must be provided for each node pair in the target path '{}'",
          reference_types_str,
          target_path)};
    }
    for (const auto& reference_type_name : reference_type_names) {
      if (auto reference_type = opc::mapOpcReferenceType(reference_type_name)) {
        reference_types.push_back(*reference_type);
      } else {
        return std::unexpected{fmt::format("unsupported reference type '{}' in target path reference types", reference_type_name)};
      }
    }
  }

  std::vector<opc::NodeId> translated_node_ids;
  if (auto sc = connection_->translateBrowsePathsToNodeIdsRequest(target_path, translated_node_ids, namespace_idx, reference_types, logger_);
      sc != UA_STATUSCODE_GOOD) {
    return std::unexpected{fmt::format("failed to translate target path '{}' to a node id: {}", target_path, UA_StatusCode_name(sc))};
  }
  if (translated_node_ids.size() != 1) {
    return std::unexpected{
        fmt::format("target path '{}' resolved to {} node ids; exactly one file node is required", target_path, translated_node_ids.size())};
  }
  return std::move(translated_node_ids[0]);
}

std::expected<opc::NodeId, std::string> PutOPCFile::findOrCreateFileInParent(core::ProcessContext& context, const core::FlowFile& flow_file) const {
  if (!parent_node_defined_) {
    return std::unexpected{"target file node does not exist and no parent node is defined to create it in"};
  }
  const auto browse_name = context.getProperty(TargetNodeBrowseName, &flow_file).value_or("");
  if (browse_name.empty()) {
    return std::unexpected{"target node browse name is required to look up or create the file in the parent node"};
  }

  // The node ID of a created file is assigned by the server, so a file created earlier is found by its browse name
  if (auto file_node = connection_->findChildByBrowseName(node_, browse_name, UA_NS0ID_HASCOMPONENT)) {
    return std::move(*file_node);
  } else if (file_node.error() != UA_STATUSCODE_BADNOMATCH) {
    return std::unexpected{fmt::format("failed to browse parent node '{}': {}", node_id_, UA_StatusCode_name(file_node.error()))};
  }

  logger_->log_debug("File '{}' not found in parent node '{}', creating it", browse_name, node_id_);
  auto created_file_node = opc::createFileInDirectory(*connection_, node_, browse_name);
  if (!created_file_node) {
    return std::unexpected{
        fmt::format("failed to create file '{}' in parent node '{}': {}", browse_name, node_id_, UA_StatusCode_name(created_file_node.error()))};
  }
  return std::move(*created_file_node);
}

std::expected<opc::NodeId, std::string> PutOPCFile::getTargetFileNode(core::ProcessContext& context, const core::FlowFile& flow_file) const {
  if (target_node_id_defined_) {
    auto target_node = findTargetFileNodeById(context, flow_file);
    if (!target_node) {
      return std::unexpected{target_node.error()};
    }
    if (*target_node) {
      return std::move(**target_node);
    }
  }
  return findOrCreateFileInParent(context, flow_file);
}

void PutOPCFile::addFileNodeAttributes(const UA_NodeId& file_node_id, core::FlowFile& flow_file) const {
  for (const auto& [key, value] : opc::nodeIdAttributes(file_node_id)) {
    flow_file.setAttribute(key, value);
  }
  if (auto browse_name = connection_->readBrowseName(file_node_id)) {
    flow_file.setAttribute(BrowsenameAttr.name, *browse_name);
  }
}

std::expected<void, std::string> PutOPCFile::writeFile(const UA_NodeId& file_node_id, core::ProcessSession& session,
    const std::shared_ptr<core::FlowFile>& flow_file) const {
  auto remote_file = opc::RemoteFile::create(*connection_, opc::NodeId::copyOf(file_node_id));
  if (!remote_file) {
    return std::unexpected{
        fmt::format("target node is not a file node, failed to find its file methods: {}", UA_StatusCode_name(remote_file.error()))};
  }

  const UA_Byte mode = opc::FILE_MODE_WRITE | (write_mode_ == OPCFileWriteMode::Append ? opc::FILE_MODE_APPEND : opc::FILE_MODE_ERASE_EXISTING);
  auto handle = remote_file->open(mode);
  if (!handle) {
    return std::unexpected{fmt::format("failed to open file for writing: {}", UA_StatusCode_name(handle.error()))};
  }
  opc::RemoteFileHandle file_handle(*remote_file, *handle);

  const auto chunk_size = gsl::narrow<size_t>(std::min<uint64_t>(chunk_size_,
      remote_file->maxByteStringLength().value_or(std::numeric_limits<UA_UInt32>::max())));
  UA_StatusCode write_status = UA_STATUSCODE_GOOD;
  session.read(flow_file, [&](const std::shared_ptr<io::InputStream>& input_stream) -> io::IoResult {
    std::vector<std::byte> buffer(chunk_size);
    size_t total_size = 0;
    while (true) {
      const auto read = input_stream->read(buffer);
      if (io::isError(read)) {
        return io::IoResult::error();
      }
      if (read == 0) {
        return io::IoResult::from(total_size);
      }
      write_status = remote_file->write(file_handle.get(), std::span<const std::byte>(buffer.data(), read));
      if (write_status != UA_STATUSCODE_GOOD) {
        return io::IoResult::from(total_size);
      }
      total_size += read;
    }
  });

  if (write_status != UA_STATUSCODE_GOOD) {
    return std::unexpected{fmt::format("failed to write file: {}", UA_StatusCode_name(write_status))};
  }
  // Servers may only persist the written content on close, so its result has to be checked
  if (auto sc = file_handle.close(); sc != UA_STATUSCODE_GOOD) {
    return std::unexpected{fmt::format("failed to close file: {}", UA_StatusCode_name(sc))};
  }
  return {};
}

void PutOPCFile::onTrigger(core::ProcessContext& context, core::ProcessSession& session) {
  logger_->log_trace("PutOPCFile::onTrigger");

  if (!reconnect()) {
    logger_->log_warn("Could not connect to OPC server, yielding");
    context.yield();
    return;
  }

  if (!resolveParentPath()) {
    context.yield();
    return;
  }

  auto flow_file = session.get();
  if (!flow_file) {
    context.yield();
    return;
  }

  auto file_node = getTargetFileNode(context, *flow_file);
  if (!file_node) {
    logger_->log_error("Flowfile {}: {}, routing to failure", flow_file->getUUIDStr(), file_node.error());
    session.transfer(flow_file, Failure);
    return;
  }

  if (auto result = writeFile(*file_node, session, flow_file); !result) {
    logger_->log_error("Flowfile {}: {}, routing to failure", flow_file->getUUIDStr(), result.error());
    session.transfer(flow_file, Failure);
    return;
  }

  addFileNodeAttributes(*file_node, *flow_file);
  session.transfer(flow_file, Success);
}

REGISTER_RESOURCE(PutOPCFile, Processor);

}  // namespace org::apache::nifi::minifi::processors
