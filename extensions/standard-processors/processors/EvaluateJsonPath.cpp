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
#include "EvaluateJsonPath.h"

#include "core/ProcessSession.h"
#include "core/ProcessContext.h"
#include "core/Resource.h"
#include "utils/ProcessorConfigUtils.h"

#include <jsoncons/json.hpp>

namespace org::apache::nifi::minifi::processors {

void EvaluateJsonPath::initialize() {
  setSupportedProperties(Properties);
  setSupportedRelationships(Relationships);
}

void EvaluateJsonPath::onSchedule(core::ProcessContext& context, core::ProcessSessionFactory&) {
  destination_ = utils::parseEnumProperty<evaluate_json_path::DestinationType>(context, EvaluateJsonPath::Destination);
  if (destination_ == evaluate_json_path::DestinationType::FlowFileContent && context.getDynamicProperties().size() > 1) {
    throw Exception(PROCESS_SCHEDULE_EXCEPTION, "Only one dynamic property is allowed for JSON path when destination is set to flowfile-content");
  }
  max_string_length_ = utils::parseDataSizeProperty(context, EvaluateJsonPath::MaxStringLength);
  null_value_representation_ = utils::parseEnumProperty<evaluate_json_path::NullValueRepresentationOption>(context, EvaluateJsonPath::NullValueRepresentation);
  return_type_ = utils::parseEnumProperty<evaluate_json_path::ReturnTypeOption>(context, EvaluateJsonPath::ReturnType);
}

void EvaluateJsonPath::onTrigger(core::ProcessContext&, core::ProcessSession& session) {
  auto flow_file = session.get();
  if (!flow_file) {
    return;
  }

  auto flow_file_read_result = session.readBuffer(flow_file);
  auto json_string = std::string(reinterpret_cast<const char*>(flow_file_read_result.buffer.data()), flow_file_read_result.buffer.size());
  if (json_string.empty()) {
    logger_->log_error("FlowFile content is empty, transferring to Failure relationship");
    session.transfer(flow_file, Failure);
    return;
  }

  jsoncons::json json_object;
  try {
    json_object = jsoncons::json::parse(json_string);
  } catch (const jsoncons::json_exception& e) {
    logger_->log_error("FlowFile content is not a valid JSON document, transferring to Failure relationship: {}", e.what());
    session.transfer(flow_file, Failure);
    return;
  }
}

REGISTER_RESOURCE(EvaluateJsonPath, Processor);

}  // namespace org::apache::nifi::minifi::processors
