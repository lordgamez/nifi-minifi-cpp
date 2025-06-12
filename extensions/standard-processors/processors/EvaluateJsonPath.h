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

#include "rapidjson/document.h"
#include "core/logging/LoggerFactory.h"
#include "core/Processor.h"
#include "core/PropertyDefinition.h"
#include "core/PropertyDefinitionBuilder.h"
#include "minifi-cpp/core/PropertyValidator.h"
#include "core/RelationshipDefinition.h"

namespace org::apache::nifi::minifi::processors {

class EvaluateJsonPath : public core::ProcessorImpl {
 public:
  EXTENSIONAPI static constexpr const char* Description = "Evaluates one or more JsonPath expressions against the content of a FlowFile. The results of those expressions are assigned to "
        "FlowFile Attributes or are written to the content of the FlowFile itself, depending on configuration of the Processor. JsonPaths are entered by adding user-defined properties; "
        "the name of the property maps to the Attribute Name into which the result will be placed (if the Destination is flowfile-attribute; otherwise, the property name is ignored). "
        "The value of the property must be a valid JsonPath expression. A Return Type of 'auto-detect' will make a determination based off the configured destination. When 'Destination' is set to "
        "'flowfile-attribute,' a return type of 'scalar' will be used. When 'Destination' is set to 'flowfile-content,' a return type of 'JSON' will be used.If the JsonPath evaluates to a JSON "
        "array or JSON object and the Return Type is set to 'scalar' the FlowFile will be unmodified and will be routed to failure. A Return Type of JSON can return scalar values if the provided "
        "JsonPath evaluates to the specified value and will be routed as a match.If Destination is 'flowfile-content' and the JsonPath does not evaluate to a defined path, the FlowFile will be "
        "routed to 'unmatched' without having its contents modified. If Destination is 'flowfile-attribute' and the expression matches nothing, attributes will be created with empty strings as the "
        "value unless 'Path Not Found Behaviour' is set to 'skip', and the FlowFile will always be routed to 'matched.'";

  EXTENSIONAPI static constexpr auto Destination = core::PropertyDefinitionBuilder<>::createProperty("Destination")
      .withDescription("Indicates whether the results of the JsonPath evaluation are written to the FlowFile content or a FlowFile attribute; if using attribute, must specify the Attribute Name "
          "property. If set to flowfile-content, only one JsonPath may be specified, and the property name is ignored.")
      .withValidator(core::StandardPropertyValidators::NON_BLANK_VALIDATOR)
      .build();
  EXTENSIONAPI static constexpr auto AttributesRegularExpression = core::PropertyDefinitionBuilder<>::createProperty("Attributes Regular Expression")
      .withDescription("Regular expression that will be evaluated against the flow file attributes to select the matching attributes. "
          "Both the matching attributes and the selected attributes from the Attributes List property will be written in the resulting JSON.")
      .withValidator(core::StandardPropertyValidators::NON_BLANK_VALIDATOR)
      .build();
  EXTENSIONAPI static constexpr auto Destination = core::PropertyDefinitionBuilder<2>::createProperty("Destination")
      .withDescription("Control if JSON value is written as a new flowfile attribute 'JSONAttributes' or written in the flowfile content. "
          "Writing to flowfile content will overwrite any existing flowfile content.")
      .isRequired(true)
      .withDefaultValue(magic_enum::enum_name(attributes_to_json::WriteDestination::FLOWFILE_ATTRIBUTE))
      .withAllowedValues(magic_enum::enum_names<attributes_to_json::WriteDestination>())
      .build();
  EXTENSIONAPI static constexpr auto IncludeCoreAttributes = core::PropertyDefinitionBuilder<>::createProperty("Include Core Attributes")
      .withDescription("Determines if the FlowFile core attributes which are contained in every FlowFile should be included in the final JSON value generated.")
      .isRequired(true)
      .withValidator(core::StandardPropertyValidators::BOOLEAN_VALIDATOR)
      .withDefaultValue("true")
      .build();
  EXTENSIONAPI static constexpr auto NullValue = core::PropertyDefinitionBuilder<>::createProperty("Null Value")
      .withDescription("If true a non existing selected attribute will be NULL in the resulting JSON. If false an empty string will be placed in the JSON.")
      .isRequired(true)
      .withValidator(core::StandardPropertyValidators::BOOLEAN_VALIDATOR)
      .withDefaultValue("false")
      .build();
  EXTENSIONAPI static constexpr auto Properties = std::to_array<core::PropertyReference>({
      AttributesList,
      AttributesRegularExpression,
      Destination,
      IncludeCoreAttributes,
      NullValue
  });


  EXTENSIONAPI static constexpr core::RelationshipDefinition Failure{"failure", "FlowFiles are routed to this relationship when the JsonPath cannot be evaluated against the content of the FlowFile; for instance, if the FlowFile is not valid JSON"};
  EXTENSIONAPI static constexpr core::RelationshipDefinition Matched{"matched", "FlowFiles are routed to this relationship when the JsonPath is successfully evaluated and the FlowFile is modified as a result"};
  EXTENSIONAPI static constexpr core::RelationshipDefinition Unmatched{"unmatched", "FlowFiles are routed to this relationship when the JsonPath does not match the content of the FlowFile and the Destination is set to flowfile-content"};
  EXTENSIONAPI static constexpr auto Relationships = std::array{Failure, Matched, Unmatched};

  EXTENSIONAPI static constexpr bool SupportsDynamicProperties = false;
  EXTENSIONAPI static constexpr bool SupportsDynamicRelationships = false;
  EXTENSIONAPI static constexpr core::annotation::Input InputRequirement = core::annotation::Input::INPUT_REQUIRED;
  EXTENSIONAPI static constexpr bool IsSingleThreaded = false;

  ADD_COMMON_VIRTUAL_FUNCTIONS_FOR_PROCESSORS

  explicit EvaluateJsonPath(const std::string_view name, const utils::Identifier& uuid = {})
      : core::ProcessorImpl(name, uuid) {
    logger_ = core::logging::LoggerFactory<EvaluateJsonPath>::getLogger(uuid_);
  }

  void initialize() override;
  void onSchedule(core::ProcessContext& context, core::ProcessSessionFactory& session_factory) override;
  void onTrigger(core::ProcessContext& context, core::ProcessSession& session) override;

 private:

};

}  // namespace org::apache::nifi::minifi::processors
