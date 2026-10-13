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
#pragma once

#include <memory>
#include <string>

#include "BaseOPCProcessor.h"
#include "OPCCommon.h"
#include "OPCFileTransfer.h"
#include "core/ProcessSession.h"
#include "core/PropertyDefinitionBuilder.h"
#include "core/logging/LoggerFactory.h"
#include "minifi-cpp/core/Property.h"
#include "minifi-cpp/core/PropertyValidator.h"
#include "minifi-cpp/core/RelationshipDefinition.h"
#include "utils/ArrayUtils.h"

namespace org::apache::nifi::minifi::processors {

class FetchOPCFile final : public BaseOPCProcessor {
 public:
  using BaseOPCProcessor::BaseOPCProcessor;

  EXTENSIONAPI static constexpr const char* Description = "Fetches the content of an OPC UA FileType node using its Open, Read and Close methods.";

  EXTENSIONAPI static constexpr auto NodeIDType =
      core::PropertyDefinitionBuilder<magic_enum::enum_count<opc::OPCNodeIDType>()>::createProperty("Node ID type")
          .withDescription("Specifies the type of the provided node ID")
          .isRequired(true)
          .withAllowedValues(magic_enum::enum_names<opc::OPCNodeIDType>())
          .build();
  EXTENSIONAPI static constexpr auto NodeID =
      core::PropertyDefinitionBuilder<>::createProperty("Node ID")
          .withDescription(
              "Specifies the ID of the file node to fetch. In case of a Path Node ID Type, the path should be provided in the format of "
              "'path/to/node'.")
          .isRequired(true)
          .build();
  EXTENSIONAPI static constexpr auto NameSpaceIndex =
      core::PropertyDefinitionBuilder<>::createProperty("Namespace index")
          .withDescription("The index of the namespace.")
          .withValidator(core::StandardPropertyValidators::UNSIGNED_INTEGER_VALIDATOR)
          .withDefaultValue("0")
          .isRequired(true)
          .build();
  EXTENSIONAPI static constexpr auto ChunkSize =
      core::PropertyDefinitionBuilder<>::createProperty("Chunk size")
          .withDescription(
              "The maximum number of bytes requested from the server in a single Read call. "
              "If the server provides a smaller MaxByteStringLength property for the file node, that value is used instead.")
          .withValidator(core::StandardPropertyValidators::DATA_SIZE_VALIDATOR)
          .withDefaultValue("32 KB")
          .isRequired(true)
          .build();
  EXTENSIONAPI static constexpr auto Properties = utils::array_cat(BaseOPCProcessor::Properties,
      std::to_array<core::PropertyReference>({NodeIDType, NodeID, NameSpaceIndex, ChunkSize}));

  EXTENSIONAPI static constexpr auto Success = core::RelationshipDefinition{"success", "Successfully fetched OPC UA file content"};
  EXTENSIONAPI static constexpr auto Relationships = std::array{Success};

  EXTENSIONAPI static constexpr auto NodeIDAttr = core::OutputAttributeDefinition<>{"NodeID", {Success}, "ID of the file node."};
  EXTENSIONAPI static constexpr auto NodeIDTypeAttr = core::OutputAttributeDefinition<>{"NodeID type", {Success}, "Type of the node ID."};
  EXTENSIONAPI static constexpr auto NamespaceIndexAttr = core::OutputAttributeDefinition<>{
      "Namespace index", {Success}, "Namespace index of the file node."};
  EXTENSIONAPI static constexpr auto BrowsenameAttr = core::OutputAttributeDefinition<>{"Browsename", {Success}, "The browse name of the file node."};
  EXTENSIONAPI static constexpr auto OutputAttributes = std::array<core::OutputAttributeReference, 4>{
      NodeIDAttr, NodeIDTypeAttr, NamespaceIndexAttr, BrowsenameAttr};

  EXTENSIONAPI static constexpr bool SupportsDynamicProperties = false;
  EXTENSIONAPI static constexpr bool SupportsDynamicRelationships = false;
  EXTENSIONAPI static constexpr core::annotation::Input InputRequirement = core::annotation::Input::INPUT_FORBIDDEN;
  EXTENSIONAPI static constexpr bool IsSingleThreaded = true;

  ADD_COMMON_VIRTUAL_FUNCTIONS_FOR_PROCESSORS

  void onSchedule(core::ProcessContext& context, core::ProcessSessionFactory& factory) override;
  void onTrigger(core::ProcessContext& context, core::ProcessSession& session) override;
  void initialize() override;

 private:
  bool resolvePath();
  bool writeOPCFileToFlowFile(opc::RemoteFile& remote_file, UA_UInt32 handle, core::ProcessSession& session, const std::shared_ptr<core::FlowFile>& flow_file);

  uint64_t chunk_size_ = 0;
};

}  // namespace org::apache::nifi::minifi::processors
