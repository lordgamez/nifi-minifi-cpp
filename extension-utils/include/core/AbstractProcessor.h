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
#include <string>
#include <string_view>
#include <type_traits>
#include "range/v3/view/transform.hpp"
#include "minifi-cpp/core/Annotation.h"
#include "core/Core.h"
#include "core/Processor.h"
#include "core/PropertyDefinition.h"
#include "minifi-cpp/core/RelationshipDefinition.h"
#include "utils/StringUtils.h"
#include "core/logging/LoggerFactory.h"

namespace org::apache::nifi::minifi::core {
template<typename ProcessorT>
class AbstractProcessor : public ProcessorImpl {
 public:
  explicit AbstractProcessor(std::string_view name, const utils::Identifier& uuid = {})
      : ProcessorImpl(name, uuid) {
    logger_ = core::logging::LoggerFactory<ProcessorT>::getLogger(uuid_);
  }

  void initialize() final {
    static_assert(std::is_same_v<typename decltype(ProcessorT::Properties)::value_type, PropertyReference>);
    static_assert(std::is_same_v<typename decltype(ProcessorT::Relationships)::value_type, RelationshipDefinition>);
    setSupportedProperties(ProcessorT::Properties);
    setSupportedRelationships(ProcessorT::Relationships);
  }

  void onSchedule(core::ProcessContext&, core::ProcessSessionFactory&) override = 0;
  void onTrigger(core::ProcessContext&, core::ProcessSession&) override = 0;

  bool supportsDynamicProperties() const noexcept final { return ProcessorT::SupportsDynamicProperties; }
  bool supportsDynamicRelationships() const noexcept final { return ProcessorT::SupportsDynamicRelationships; }
  minifi::core::annotation::Input getInputRequirement() const noexcept final { return ProcessorT::InputRequirement; }
  bool isSingleThreaded() const noexcept final { return ProcessorT::IsSingleThreaded; }
  std::string getProcessorType() const final {
    return utils::string::partAfterLastOccurrenceOf(className<ProcessorT>(), ':');
  }
};
}  // namespace org::apache::nifi::minifi::core
