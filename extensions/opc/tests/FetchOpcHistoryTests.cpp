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
#include "include/FetchOpcHistory.h"
#include "unit/TestUtils.h"

namespace org::apache::nifi::minifi::test {

TEST_CASE("Test fetching history of node with a single entry", "[fetchopchistory]") {
  OpcUaTestServer server(4841);
  server.start();
  SingleProcessorTestController controller{minifi::test::utils::make_processor<processors::FetchOpcHistory>("FetchOpcHistory")};
  auto fetch_opc_processor = controller.getProcessor();
  REQUIRE(fetch_opc_processor->setProperty(processors::FetchOpcHistory::OPCServerEndPoint.name, "opc.tcp://127.0.0.1:4841/"));
  REQUIRE(fetch_opc_processor->setProperty(processors::FetchOpcHistory::NodeIDType.name, "String"));
  REQUIRE(fetch_opc_processor->setProperty(processors::FetchOpcHistory::NodeID.name, "INT1"));
  REQUIRE(fetch_opc_processor->setProperty(processors::FetchOpcHistory::NameSpaceIndex.name, std::to_string(server.getNamespaceIndex())));

  bool contains_modification_attributes = false;
  SECTION("Fetch full history") {
    contains_modification_attributes = true;
    REQUIRE(fetch_opc_processor->setProperty(processors::FetchOpcHistory::HistoryReadType.name, "Modified"));
  }

  SECTION("Fetch raw history") {
    contains_modification_attributes = false;
  }

  const auto results = controller.trigger();
  REQUIRE(results.at(processors::FetchOpcHistory::Success).size() == 1);
  auto flow_file = results.at(processors::FetchOpcHistory::Success)[0];
  CHECK(controller.plan->getContent(flow_file) == "1");
  if (contains_modification_attributes) {
    CHECK(flow_file->getAttribute("ModificationUsername") == "test_user");
    CHECK(flow_file->getAttribute("ModificationUpdateType") == "Replace");
    CHECK(flow_file->getAttribute("ModificationTime") == "2024-06-15T10:30:00.000Z");
  } else {
    CHECK(flow_file->getAttribute("ModificationUsername") == std::nullopt);
    CHECK(flow_file->getAttribute("ModificationUpdateType") == std::nullopt);
    CHECK(flow_file->getAttribute("ModificationTime") == std::nullopt);
  }
}

TEST_CASE("Test fetching history after a specific timestamp", "[fetchopchistory]") {
  OpcUaTestServer server(4841);
  server.start();
  SingleProcessorTestController controller{minifi::test::utils::make_processor<processors::FetchOpcHistory>("FetchOpcHistory")};
  auto fetch_opc_processor = controller.getProcessor();
  REQUIRE(fetch_opc_processor->setProperty(processors::FetchOpcHistory::OPCServerEndPoint.name, "opc.tcp://127.0.0.1:4841/"));
  REQUIRE(fetch_opc_processor->setProperty(processors::FetchOpcHistory::NodeIDType.name, "String"));
  REQUIRE(fetch_opc_processor->setProperty(processors::FetchOpcHistory::NodeID.name, "INT2"));
  REQUIRE(fetch_opc_processor->setProperty(processors::FetchOpcHistory::NameSpaceIndex.name, std::to_string(server.getNamespaceIndex())));
  REQUIRE(fetch_opc_processor->setProperty(processors::FetchOpcHistory::StartTimestamp.name, "2025-10-01T00:00:00Z"));

  bool contains_modification_attributes = false;
  SECTION("Fetch full history") {
    contains_modification_attributes = true;
    REQUIRE(fetch_opc_processor->setProperty(processors::FetchOpcHistory::HistoryReadType.name, "Modified"));
  }

  SECTION("Fetch raw history") {
    contains_modification_attributes = false;
  }

  const auto results = controller.trigger();
  REQUIRE(results.at(processors::FetchOpcHistory::Success).size() == 2);
  auto flow_file = results.at(processors::FetchOpcHistory::Success)[0];
  CHECK(controller.plan->getContent(flow_file) == "3");
  if (contains_modification_attributes) {
    CHECK(flow_file->getAttribute("ModificationUsername") == "admin_user");
    CHECK(flow_file->getAttribute("ModificationUpdateType") == "Update");
    CHECK(flow_file->getAttribute("ModificationTime") == "2025-11-11T11:30:00.000Z");
  } else {
    CHECK(flow_file->getAttribute("ModificationUsername") == std::nullopt);
    CHECK(flow_file->getAttribute("ModificationUpdateType") == std::nullopt);
    CHECK(flow_file->getAttribute("ModificationTime") == std::nullopt);
  }
  flow_file = results.at(processors::FetchOpcHistory::Success)[1];
  CHECK(controller.plan->getContent(flow_file) == "4");
  if (contains_modification_attributes) {
    CHECK(flow_file->getAttribute("ModificationUsername") == "test_user");
    CHECK(flow_file->getAttribute("ModificationUpdateType") == "Replace");
    CHECK(flow_file->getAttribute("ModificationTime") == "2026-03-11T11:30:00.000Z");
  } else {
    CHECK(flow_file->getAttribute("ModificationUsername") == std::nullopt);
    CHECK(flow_file->getAttribute("ModificationUpdateType") == std::nullopt);
    CHECK(flow_file->getAttribute("ModificationTime") == std::nullopt);
  }
}

TEST_CASE("Test fetching history before a specific timestamp", "[fetchopchistory]") {
  OpcUaTestServer server(4841);
  server.start();
  SingleProcessorTestController controller{minifi::test::utils::make_processor<processors::FetchOpcHistory>("FetchOpcHistory")};
  auto fetch_opc_processor = controller.getProcessor();
  REQUIRE(fetch_opc_processor->setProperty(processors::FetchOpcHistory::OPCServerEndPoint.name, "opc.tcp://127.0.0.1:4841/"));
  REQUIRE(fetch_opc_processor->setProperty(processors::FetchOpcHistory::NodeIDType.name, "String"));
  REQUIRE(fetch_opc_processor->setProperty(processors::FetchOpcHistory::NodeID.name, "INT2"));
  REQUIRE(fetch_opc_processor->setProperty(processors::FetchOpcHistory::NameSpaceIndex.name, std::to_string(server.getNamespaceIndex())));
  REQUIRE(fetch_opc_processor->setProperty(processors::FetchOpcHistory::EndTimestamp.name, "2025-11-12T00:00:00Z"));

  bool contains_modification_attributes = false;
  SECTION("Fetch full history") {
    contains_modification_attributes = true;
    REQUIRE(fetch_opc_processor->setProperty(processors::FetchOpcHistory::HistoryReadType.name, "Modified"));
  }

  SECTION("Fetch raw history") {
    contains_modification_attributes = false;
  }

  const auto results = controller.trigger();
  REQUIRE(results.at(processors::FetchOpcHistory::Success).size() == 2);
  auto flow_file = results.at(processors::FetchOpcHistory::Success)[0];
  CHECK(controller.plan->getContent(flow_file) == "2");
  if (contains_modification_attributes) {
    CHECK(flow_file->getAttribute("ModificationUsername") == "admin_user");
    CHECK(flow_file->getAttribute("ModificationUpdateType") == "Insert");
    CHECK(flow_file->getAttribute("ModificationTime") == "2021-03-15T11:30:00.000Z");
  } else {
    CHECK(flow_file->getAttribute("ModificationUsername") == std::nullopt);
    CHECK(flow_file->getAttribute("ModificationUpdateType") == std::nullopt);
    CHECK(flow_file->getAttribute("ModificationTime") == std::nullopt);
  }
  flow_file = results.at(processors::FetchOpcHistory::Success)[1];
  CHECK(controller.plan->getContent(flow_file) == "3");
  if (contains_modification_attributes) {
    CHECK(flow_file->getAttribute("ModificationUsername") == "admin_user");
    CHECK(flow_file->getAttribute("ModificationUpdateType") == "Update");
    CHECK(flow_file->getAttribute("ModificationTime") == "2025-11-11T11:30:00.000Z");
  } else {
    CHECK(flow_file->getAttribute("ModificationUsername") == std::nullopt);
    CHECK(flow_file->getAttribute("ModificationUpdateType") == std::nullopt);
    CHECK(flow_file->getAttribute("ModificationTime") == std::nullopt);
  }
}

TEST_CASE("Test batch size limit", "[fetchopchistory]") {

}

TEST_CASE("Test JSON output format", "[fetchopchistory]") {

}

TEST_CASE("Test multiple triggers with state kept in state manager", "[fetchopchistory]") {

}

// TODO: add and test additional output attributes

}  // namespace org::apache::nifi::minifi::test
