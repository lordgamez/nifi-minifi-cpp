/**
 * @file AttributesToJSON.h
 * AttributesToJSON class declaration
 *
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

#include "core/Processor.h"
#include "core/Property.h"
#include "core/logging/Logger.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace processors {

class AttributesToJSON : public core::Processor {
 public:
  AttributesToJSON(const std::string& name, const utils::Identifier& uuid = {})
      : core::Processor(name, uuid),
        logger_(logging::LoggerFactory<AttributesToJSON>::getLogger()) {
  }
  static constexpr char const* ProcessorName = "AttributesToJSON";
  // Supported Properties
  static const core::Property AttributesList;
  static const core::Property AttributesRegularExpression;
  static const core::Property Destination;
  static const core::Property IncludeCoreAttributes;
  static const core::Property NullValue;

  // Supported Relationships
  static core::Relationship Success;
  static core::Relationship Failure;

  void initialize() override;
  void onSchedule(core::ProcessContext *context, core::ProcessSessionFactory* sessionFactory) override;
  void onTrigger(core::ProcessContext *context, core::ProcessSession *session) override;

  core::annotation::Input getInputRequirement() const override {
    return core::annotation::Input::INPUT_REQUIRED;
  }

 private:
  std::shared_ptr<logging::Logger> logger_;
  std::string attributes_list_;
  std::string attributes_regular_expression_;
  std::string destination_;
  bool include_core_attributes_ = true;
  bool null_value_ = false;
};

REGISTER_RESOURCE(AttributesToJSON, "Generates a JSON representation of the input FlowFile Attributes. The resulting JSON can be written to either a new Attribute 'JSONAttributes' or written to the FlowFile as content.");

}  // namespace processors
}  // namespace minifi
}  // namespace nifi
}  // namespace apache
}  // namespace org
