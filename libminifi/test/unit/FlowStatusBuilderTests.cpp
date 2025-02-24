/**
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
#include "unit/TestBase.h"
#include "unit/Catch.h"
#include "c2/FlowStatusBuilder.h"

namespace org::apache::nifi::minifi::test {

TEST_CASE("Parse invalid flow status query string", "[flowstatusbuilder]") {
  REQUIRE_THROWS_WITH(c2::FlowStatusRequest("invalid query string"), "Invalid query string: invalid query string");
}

TEST_CASE("Parse invalid flow status query type", "[flowstatusbuilder]") {
  REQUIRE_THROWS_WITH(c2::FlowStatusRequest("invalid_type:TaiFile:health"), "Invalid query type: invalid_type");
}

TEST_CASE("Parse two part flow status query", "[flowstatusbuilder]") {
  c2::FlowStatusRequest request("processor:health,status");
  REQUIRE(request.query_type == c2::FlowStatusQueryType::processor);
  REQUIRE(request.identifier.empty());
  REQUIRE(request.options == std::unordered_set<std::string>{"health", "status"});
}

TEST_CASE("Parse three part flow status query", "[flowstatusbuilder]") {
  c2::FlowStatusRequest request("processor:TailFile:health");
  REQUIRE(request.query_type == c2::FlowStatusQueryType::processor);
  REQUIRE(request.identifier == "TailFile");
  REQUIRE(request.options == std::unordered_set<std::string>{"health"});
}

TEST_CASE("Build empty flow status", "[flowstatusbuilder]") {
  c2::FlowStatusBuilder flow_status_builder;
  REQUIRE(flow_status_builder.buildFlowStatus({}) == rapidjson::Value());
}

}  // namespace org::apache::nifi::minifi::test
