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
#include "OpcUaTestServer.h"
#include "unit/SingleProcessorTestController.h"
#include "include/putopc.h"

namespace org::apache::nifi::minifi::test {

TEST_CASE("Test creating a new node with path node id", "[putopcprocessor]") {
  OpcUaTestServer server;
  server.start();
  SingleProcessorTestController controller{std::make_unique<processors::PutOPCProcessor>("PutOPCProcessor")};
  auto put_opc_processor = controller.getProcessor();
  put_opc_processor->setProperty(processors::PutOPCProcessor::OPCServerEndPoint, "opc.tcp://127.0.0.1:4840/");
  put_opc_processor->setProperty(processors::PutOPCProcessor::ParentNodeIDType, "Path");
  put_opc_processor->setProperty(processors::PutOPCProcessor::ParentNodeID, "Simulator/Default/Device1");
  put_opc_processor->setProperty(processors::PutOPCProcessor::ParentNameSpaceIndex, std::to_string(server.getNamespaceIndex()));
  put_opc_processor->setProperty(processors::PutOPCProcessor::ValueType, "Int32");
  put_opc_processor->setProperty(processors::PutOPCProcessor::TargetNodeIDType, "Int");
  put_opc_processor->setProperty(processors::PutOPCProcessor::TargetNodeID, "9999");
  put_opc_processor->setProperty(processors::PutOPCProcessor::TargetNodeNameSpaceIndex, "2");
  put_opc_processor->setProperty(processors::PutOPCProcessor::TargetNodeBrowseName, "everything");

  const auto results = controller.trigger("42");
  REQUIRE(results.at(processors::PutOPCProcessor::Failure).empty());
  REQUIRE(results.at(processors::PutOPCProcessor::Success).size() == 1);
  auto flow_file = results.at(processors::PutOPCProcessor::Success)[0];
  CHECK(controller.plan->getContent(flow_file) == "42");
}

TEST_CASE("Test fetching using custom reference type id path", "[putopcprocessor]") {
  OpcUaTestServer server;
  server.start();
  SingleProcessorTestController controller{std::make_unique<processors::PutOPCProcessor>("PutOPCProcessor")};
  auto put_opc_processor = controller.getProcessor();
  put_opc_processor->setProperty(processors::PutOPCProcessor::OPCServerEndPoint, "opc.tcp://127.0.0.1:4840/");
  put_opc_processor->setProperty(processors::PutOPCProcessor::ParentNodeIDType, "Path");
  put_opc_processor->setProperty(processors::PutOPCProcessor::ParentNodeID, "Simulator/Default/Device1/INT3/INT4");
  put_opc_processor->setProperty(processors::PutOPCProcessor::PathReferenceTypes, "Organizes/Organizes/HasComponent/HasComponent");
  put_opc_processor->setProperty(processors::PutOPCProcessor::ParentNameSpaceIndex, std::to_string(server.getNamespaceIndex()));
  put_opc_processor->setProperty(processors::PutOPCProcessor::ValueType, "Int32");
  put_opc_processor->setProperty(processors::PutOPCProcessor::TargetNodeIDType, "Int");
  put_opc_processor->setProperty(processors::PutOPCProcessor::TargetNodeID, "9999");
  put_opc_processor->setProperty(processors::PutOPCProcessor::TargetNodeNameSpaceIndex, "2");
  put_opc_processor->setProperty(processors::PutOPCProcessor::TargetNodeBrowseName, "everything");

  const auto results = controller.trigger("42");
  REQUIRE(results.at(processors::PutOPCProcessor::Failure).empty());
  REQUIRE(results.at(processors::PutOPCProcessor::Success).size() == 1);
  auto flow_file = results.at(processors::PutOPCProcessor::Success)[0];
  CHECK(controller.plan->getContent(flow_file) == "42");
}

TEST_CASE("Test missing path reference types", "[putopcprocessor]") {
  SingleProcessorTestController controller{std::make_unique<processors::PutOPCProcessor>("PutOPCProcessor")};
  auto put_opc_processor = controller.getProcessor();
  put_opc_processor->setProperty(processors::PutOPCProcessor::OPCServerEndPoint, "opc.tcp://127.0.0.1:4840/");
  put_opc_processor->setProperty(processors::PutOPCProcessor::ParentNodeIDType, "Path");
  put_opc_processor->setProperty(processors::PutOPCProcessor::ParentNodeID, "Simulator/Default/Device1/INT3/INT4");
  put_opc_processor->setProperty(processors::PutOPCProcessor::PathReferenceTypes, "Organizes/Organizes/HasComponent");
  put_opc_processor->setProperty(processors::PutOPCProcessor::ValueType, "Int32");
  put_opc_processor->setProperty(processors::PutOPCProcessor::TargetNodeIDType, "Int");
  put_opc_processor->setProperty(processors::PutOPCProcessor::TargetNodeID, "9999");
  put_opc_processor->setProperty(processors::PutOPCProcessor::TargetNodeNameSpaceIndex, "2");
  put_opc_processor->setProperty(processors::PutOPCProcessor::TargetNodeBrowseName, "everything");

  REQUIRE_THROWS_WITH(controller.trigger("42"), "Process Schedule Operation: Path reference types must be provided for each node pair in the path!");
}

TEST_CASE("Test namespace being required", "[putopcprocessor]") {
  OpcUaTestServer server;
  server.start();
  LogTestController::getInstance().setTrace<processors::PutOPCProcessor>();
  SingleProcessorTestController controller{std::make_unique<processors::PutOPCProcessor>("PutOPCProcessor")};
  auto put_opc_processor = controller.getProcessor();
  put_opc_processor->setProperty(processors::PutOPCProcessor::OPCServerEndPoint, "opc.tcp://127.0.0.1:4840/");
  put_opc_processor->setProperty(processors::PutOPCProcessor::ParentNodeIDType, "Path");
  put_opc_processor->setProperty(processors::PutOPCProcessor::ParentNodeID, "Simulator/Default/Device1/INT3/INT4");
  put_opc_processor->setProperty(processors::PutOPCProcessor::PathReferenceTypes, "Organizes/Organizes/HasComponent/HasComponent");
  put_opc_processor->setProperty(processors::PutOPCProcessor::ParentNameSpaceIndex, std::to_string(server.getNamespaceIndex()));
  put_opc_processor->setProperty(processors::PutOPCProcessor::ValueType, "Int32");
  put_opc_processor->setProperty(processors::PutOPCProcessor::TargetNodeBrowseName, "everything");

  const auto results = controller.trigger("42");
  REQUIRE(results.at(processors::PutOPCProcessor::Success).empty());
  REQUIRE(results.at(processors::PutOPCProcessor::Failure).size() == 1);
  auto flow_file = results.at(processors::PutOPCProcessor::Failure)[0];
  CHECK(controller.plan->getContent(flow_file) == "42");
  REQUIRE(LogTestController::getInstance().contains("had no target namespace index specified, routing to failure!"));
}

}  // namespace org::apache::nifi::minifi::test
