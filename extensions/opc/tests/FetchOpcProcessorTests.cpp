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
#include "include/fetchopc.h"

namespace org::apache::nifi::minifi::test {

TEST_CASE("Test fetching using path node id", "[fetchopcprocessor]") {
  OpcUaTestServer server;
  server.start();
  SingleProcessorTestController controller{std::make_unique<processors::FetchOPCProcessor>("FetchOPCProcessor")};
  auto fetch_opc_processor = controller.getProcessor();
  fetch_opc_processor->setProperty(processors::FetchOPCProcessor::OPCServerEndPoint, "opc.tcp://127.0.0.1:4840/");
  fetch_opc_processor->setProperty(processors::FetchOPCProcessor::NodeIDType, "Path");
  fetch_opc_processor->setProperty(processors::FetchOPCProcessor::NodeID, "Simulator/Default/Device1");
  fetch_opc_processor->setProperty(processors::FetchOPCProcessor::NameSpaceIndex, std::to_string(server.getNamespaceIndex()));

  const auto results = controller.trigger();
  REQUIRE(results.at(processors::FetchOPCProcessor::Failure).empty());
  REQUIRE(results.at(processors::FetchOPCProcessor::Success).size() == 4);
  for (size_t i = 0; i < 3; i++) {
    auto flow_file = results.at(processors::FetchOPCProcessor::Success)[i];
    CHECK(flow_file->getAttribute("Browsename") == "INT" + std::to_string(i + 1));
    CHECK(flow_file->getAttribute("Datasize") == "4");
    CHECK(flow_file->getAttribute("Full path") == "Simulator/Default/Device1/INT" + std::to_string(i + 1));
    CHECK(flow_file->getAttribute("NodeID"));
    CHECK(flow_file->getAttribute("NodeID type") == "numeric");
    CHECK(flow_file->getAttribute("Typename") == "Int32");
    CHECK(controller.plan->getContent(flow_file) == std::to_string(i + 1));
  }

  auto flow_file = results.at(processors::FetchOPCProcessor::Success)[3];
  CHECK(flow_file->getAttribute("Browsename") == "INT4");
  CHECK(flow_file->getAttribute("Datasize") == "4");
  CHECK(flow_file->getAttribute("Full path") == "Simulator/Default/Device1/INT3/INT4");
  CHECK(flow_file->getAttribute("NodeID"));
  CHECK(flow_file->getAttribute("NodeID type") == "numeric");
  CHECK(flow_file->getAttribute("Typename") == "Int32");
  CHECK(controller.plan->getContent(flow_file) == "4");
}

TEST_CASE("Test fetching using custom reference type id path", "[fetchopcprocessor]") {
  OpcUaTestServer server;
  server.start();
  SingleProcessorTestController controller{std::make_unique<processors::FetchOPCProcessor>("FetchOPCProcessor")};
  auto fetch_opc_processor = controller.getProcessor();
  fetch_opc_processor->setProperty(processors::FetchOPCProcessor::OPCServerEndPoint, "opc.tcp://127.0.0.1:4840/");
  fetch_opc_processor->setProperty(processors::FetchOPCProcessor::NodeIDType, "Path");
  fetch_opc_processor->setProperty(processors::FetchOPCProcessor::NodeID, "Simulator/Default/Device1/INT3");
  fetch_opc_processor->setProperty(processors::FetchOPCProcessor::NameSpaceIndex, std::to_string(server.getNamespaceIndex()));
  fetch_opc_processor->setProperty(processors::FetchOPCProcessor::PathReferenceTypes, "Organizes/Organizes/HasComponent");

  const auto results = controller.trigger();
  REQUIRE(results.at(processors::FetchOPCProcessor::Failure).empty());
  REQUIRE(results.at(processors::FetchOPCProcessor::Success).size() == 2);
  auto flow_file = results.at(processors::FetchOPCProcessor::Success)[0];
  CHECK(flow_file->getAttribute("Browsename") == "INT3");
  CHECK(flow_file->getAttribute("Datasize") == "4");
  CHECK(flow_file->getAttribute("Full path") == "Simulator/Default/Device1/INT3");
  CHECK(flow_file->getAttribute("NodeID"));
  CHECK(flow_file->getAttribute("NodeID type") == "numeric");
  CHECK(flow_file->getAttribute("Typename") == "Int32");
  flow_file = results.at(processors::FetchOPCProcessor::Success)[1];
  CHECK(flow_file->getAttribute("Browsename") == "INT4");
  CHECK(flow_file->getAttribute("Datasize") == "4");
  CHECK(flow_file->getAttribute("Full path") == "Simulator/Default/Device1/INT3/INT4");
  CHECK(flow_file->getAttribute("NodeID"));
  CHECK(flow_file->getAttribute("NodeID type") == "numeric");
  CHECK(flow_file->getAttribute("Typename") == "Int32");
}

TEST_CASE("Test missing path reference types", "[fetchopcprocessor]") {
  SingleProcessorTestController controller{std::make_unique<processors::FetchOPCProcessor>("FetchOPCProcessor")};
  auto fetch_opc_processor = controller.getProcessor();
  fetch_opc_processor->setProperty(processors::FetchOPCProcessor::OPCServerEndPoint, "opc.tcp://127.0.0.1:4840/");
  fetch_opc_processor->setProperty(processors::FetchOPCProcessor::NodeIDType, "Path");
  fetch_opc_processor->setProperty(processors::FetchOPCProcessor::NodeID, "Simulator/Default/Device1/INT3");
  fetch_opc_processor->setProperty(processors::FetchOPCProcessor::PathReferenceTypes, "Organizes/Organizes");
  REQUIRE_THROWS_WITH(controller.trigger(), "Process Schedule Operation: Path reference types must be provided for each node pair in the path!");
}

}  // namespace org::apache::nifi::minifi::test
