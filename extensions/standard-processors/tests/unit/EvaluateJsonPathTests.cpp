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
#include "unit/TestBase.h"
#include "unit/Catch.h"
#include "unit/SingleProcessorTestController.h"
#include "processors/EvaluateJsonPath.h"
#include "unit/TestUtils.h"

namespace org::apache::nifi::minifi::test {

TEST_CASE("When destination is set to flowfile content only one dynamic property is allowed", "[EvaluateJsonPathTests]") {
  SingleProcessorTestController controller(std::make_unique<processors::EvaluateJsonPath>("EvaluateJsonPath"));
  auto evaluate_json_path = dynamic_cast<processors::EvaluateJsonPath*>(controller.getProcessor());
  REQUIRE(evaluate_json_path);
  controller.plan->setProperty(evaluate_json_path, processors::EvaluateJsonPath::Destination, "flowfile-content");
  controller.plan->setDynamicProperty(evaluate_json_path, "attribute1", "value1");
  controller.plan->setDynamicProperty(evaluate_json_path, "attribute2", "value2");
  REQUIRE_THROWS_WITH(controller.trigger({{.content = "foo"}}), "Process Schedule Operation: Only one dynamic property is allowed for JSON path when destination is set to flowfile-content");
}

TEST_CASE("Input flowfile has invalid JSON as content", "[EvaluateJsonPathTests]") {
  SingleProcessorTestController controller(std::make_unique<processors::EvaluateJsonPath>("EvaluateJsonPath"));
  LogTestController::getInstance().setTrace<processors::EvaluateJsonPath>();
  auto evaluate_json_path = dynamic_cast<processors::EvaluateJsonPath*>(controller.getProcessor());
  REQUIRE(evaluate_json_path);

  ProcessorTriggerResult result;
  std::string error_log;
  SECTION("Flow file content is empty") {
    result = controller.trigger({{.content = ""}});
    error_log = "FlowFile content is empty, transferring to Failure relationship";
  }

  SECTION("Flow file content is invalid json") {
    result = controller.trigger({{.content = "invalid json"}});
    error_log = "FlowFile content is not a valid JSON document, transferring to Failure relationship";
  }

  CHECK(result.at(processors::EvaluateJsonPath::Matched).empty());
  CHECK(result.at(processors::EvaluateJsonPath::Unmatched).empty());
  CHECK(result.at(processors::EvaluateJsonPath::Failure).size() == 1);
  CHECK(utils::verifyLogLinePresenceInPollTime(1s, error_log));
}

TEST_CASE("JSON paths are not found in content when destination is set to attribute", "[EvaluateJsonPathTests]") {
  SingleProcessorTestController controller(std::make_unique<processors::EvaluateJsonPath>("EvaluateJsonPath"));
  LogTestController::getInstance().setTrace<processors::EvaluateJsonPath>();
  auto evaluate_json_path = dynamic_cast<processors::EvaluateJsonPath*>(controller.getProcessor());
  REQUIRE(evaluate_json_path);
  controller.plan->setDynamicProperty(evaluate_json_path, "attribute1", "$.firstName");
  controller.plan->setDynamicProperty(evaluate_json_path, "attribute2", "$.lastName");

  std::map<std::string, std::string> expected_attributes = {
    {"attribute1", ""},
    {"attribute2", ""}
  };
  bool warn_path_not_found_behavior = false;
  bool expect_attributes = false;
  SECTION("Ignore path not found behavior") {
    controller.plan->setProperty(evaluate_json_path, processors::EvaluateJsonPath::PathNotFoundBehavior, "ignore");
    expect_attributes = true;
  }

  SECTION("Skip path not found behavior") {
    controller.plan->setProperty(evaluate_json_path, processors::EvaluateJsonPath::PathNotFoundBehavior, "skip");
  }

  SECTION("Warn path not found behavior") {
    controller.plan->setProperty(evaluate_json_path, processors::EvaluateJsonPath::PathNotFoundBehavior, "warn");
    warn_path_not_found_behavior = true;
    expect_attributes = true;
  }

  auto result = controller.trigger({{.content = "{}"}});

  CHECK(result.at(processors::EvaluateJsonPath::Matched).size() == 1);
  CHECK(result.at(processors::EvaluateJsonPath::Unmatched).empty());
  CHECK(result.at(processors::EvaluateJsonPath::Failure).empty());

  const auto result_flow_file = result.at(processors::EvaluateJsonPath::Matched).at(0);

  CHECK(controller.plan->getContent(result_flow_file) == "{}");

  for (const auto& [key, value] : expected_attributes) {
    std::string attribute_value;
    if (!expect_attributes) {
      CHECK_FALSE(result_flow_file->getAttribute(key, attribute_value));
    } else {
      CHECK(result_flow_file->getAttribute(key, attribute_value));
      CHECK(attribute_value == value);
    }
  }

  if (warn_path_not_found_behavior) {
    CHECK(utils::verifyLogLinePresenceInPollTime(0s, "JSON path '$.firstName' not found for attribute key 'attribute1'"));
    CHECK(utils::verifyLogLinePresenceInPollTime(0s, "JSON path '$.lastName' not found for attribute key 'attribute2'"));
  }
}

TEST_CASE("JSON paths are not found in content when destination is set in content", "[EvaluateJsonPathTests]") {
  SingleProcessorTestController controller(std::make_unique<processors::EvaluateJsonPath>("EvaluateJsonPath"));
  LogTestController::getInstance().setTrace<processors::EvaluateJsonPath>();
  auto evaluate_json_path = dynamic_cast<processors::EvaluateJsonPath*>(controller.getProcessor());
  REQUIRE(evaluate_json_path);
  controller.plan->setProperty(evaluate_json_path, processors::EvaluateJsonPath::Destination, "flowfile-content");
  controller.plan->setDynamicProperty(evaluate_json_path, "attribute", "$.firstName");

  bool warn_path_not_found_behavior = false;
  SECTION("Ignore path not found behavior") {
    controller.plan->setProperty(evaluate_json_path, processors::EvaluateJsonPath::PathNotFoundBehavior, "ignore");
  }

  SECTION("Skip path not found behavior") {
    controller.plan->setProperty(evaluate_json_path, processors::EvaluateJsonPath::PathNotFoundBehavior, "skip");
  }

  SECTION("Warn path not found behavior") {
    controller.plan->setProperty(evaluate_json_path, processors::EvaluateJsonPath::PathNotFoundBehavior, "warn");
    warn_path_not_found_behavior = true;
  }

  auto result = controller.trigger({{.content = "{}"}});

  CHECK(result.at(processors::EvaluateJsonPath::Matched).empty());
  CHECK(result.at(processors::EvaluateJsonPath::Unmatched).size() == 1);
  CHECK(result.at(processors::EvaluateJsonPath::Failure).empty());

  const auto result_flow_file = result.at(processors::EvaluateJsonPath::Unmatched).at(0);

  CHECK(controller.plan->getContent(result_flow_file) == "{}");

  std::string attribute_value;
  CHECK_FALSE(result_flow_file->getAttribute("attribute", attribute_value));

  if (warn_path_not_found_behavior) {
    CHECK(utils::verifyLogLinePresenceInPollTime(0s, "JSON path '$.firstName' not found for attribute key 'attribute'"));
  }
}

}  // namespace org::apache::nifi::minifi::test
