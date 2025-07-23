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
#include <string_view>
#include <utility>

#include "core/AbstractProcessor.h"
#include "core/Processor.h"
#include "core/ProcessSession.h"
#include "core/PropertyDefinitionBuilder.h"
#include "core/RelationshipDefinition.h"
#include "controllers/RecordSetReader.h"
#include "controllers/RecordSetWriter.h"

namespace org::apache::nifi::minifi::processors {

class ConvertRecord : public core::AbstractProcessor<ConvertRecord> {
 public:
  using core::AbstractProcessor<ConvertRecord>::AbstractProcessor;

  EXTENSIONAPI static constexpr const char* Description = "Converts records from one data format to another using configured Record Reader and Record Write Controller Services.";

  EXTENSIONAPI static constexpr auto RecordReader = core::PropertyDefinitionBuilder<>::createProperty("Record Reader")
      .withDescription("Specifies the Controller Service to use for reading incoming data")
      .isRequired(true)
      .withAllowedTypes<minifi::core::RecordSetReader>()
      .build();
  EXTENSIONAPI static constexpr auto RecordWriter = core::PropertyDefinitionBuilder<>::createProperty("Record Writer")
      .withDescription("Specifies the Controller Service to use for writing out the records")
      .isRequired(true)
      .withAllowedTypes<minifi::core::RecordSetWriter>()
      .build();

  EXTENSIONAPI static constexpr auto Properties = std::to_array<core::PropertyReference>({
      RecordReader,
      RecordWriter
  });

  EXTENSIONAPI static constexpr auto Failure = core::RelationshipDefinition{"failure",
    "If a FlowFile cannot be transformed from the configured input format to the configured output format, the unchanged FlowFile will be routed to this relationship"};
  EXTENSIONAPI static constexpr auto Success = core::RelationshipDefinition{"success", "FlowFiles that are successfully transformed will be routed to this relationship"};
  EXTENSIONAPI static constexpr auto Relationships = std::array{Failure, Success};

  EXTENSIONAPI static constexpr bool SupportsDynamicProperties = false;
  EXTENSIONAPI static constexpr bool SupportsDynamicRelationships = false;
  EXTENSIONAPI static constexpr core::annotation::Input InputRequirement = core::annotation::Input::INPUT_REQUIRED;
  EXTENSIONAPI static constexpr bool IsSingleThreaded = false;

  void onSchedule(core::ProcessContext& context, core::ProcessSessionFactory& session_factory) override;
  void onTrigger(core::ProcessContext& context, core::ProcessSession& session) override;

 private:
  std::shared_ptr<core::RecordSetReader> record_set_reader_;
  std::shared_ptr<core::RecordSetWriter> record_set_writer_;
};

}  // namespace org::apache::nifi::minifi::processors
