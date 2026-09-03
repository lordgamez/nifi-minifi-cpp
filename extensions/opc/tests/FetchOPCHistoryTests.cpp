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
#include "include/FetchOPCHistory.h"
#include "unit/TestUtils.h"
#include "catch2/generators/catch_generators.hpp"

namespace org::apache::nifi::minifi::test {

void verifyResults(SingleProcessorTestController& controller, const ProcessorTriggerResult& results, const std::string& expected_contents) {
  auto& fetch_results = results.at(processors::FetchOPCHistory::Success);
  REQUIRE(fetch_results.size() == 1);
  rapidjson::Document result_document;
  result_document.Parse(controller.plan->getContent(fetch_results[0]).c_str());
  rapidjson::Document expected_document;
  expected_document.Parse(expected_contents.c_str());
  REQUIRE(result_document == expected_document);
}

TEST_CASE("Test fetching history of node with a single entry", "[fetchopchistory]") {
  OpcUaTestServer server(4841);
  server.start();
  SingleProcessorTestController controller{minifi::test::utils::make_processor<processors::FetchOPCHistory>("FetchOPCHistory")};
  auto fetch_opc_processor = controller.getProcessor();
  REQUIRE(fetch_opc_processor->setProperty(processors::FetchOPCHistory::OPCServerEndPoint.name, "opc.tcp://127.0.0.1:4841/"));
  REQUIRE(fetch_opc_processor->setProperty(processors::FetchOPCHistory::NodeIDType.name, "String"));
  REQUIRE(fetch_opc_processor->setProperty(processors::FetchOPCHistory::NodeID.name, "INT1"));
  REQUIRE(fetch_opc_processor->setProperty(processors::FetchOPCHistory::NameSpaceIndex.name, std::to_string(server.getNamespaceIndex())));

  const auto contains_modification_attributes = GENERATE(true, false);
  if (contains_modification_attributes) {
    REQUIRE(fetch_opc_processor->setProperty(processors::FetchOPCHistory::HistoryReadType.name, "Modified"));
  }

  const auto results = controller.trigger();
  REQUIRE(results.at(processors::FetchOPCHistory::Success).size() == 1);
  auto flow_file = results.at(processors::FetchOPCHistory::Success)[0];
  CHECK(controller.plan->getContent(flow_file) == "1");

  CHECK(flow_file->getAttribute("NodeID") == "INT1");
  CHECK(flow_file->getAttribute("NamespaceIndex") == std::to_string(server.getNamespaceIndex()));
  CHECK(flow_file->getAttribute("Sourcetimestamp") == "2024-06-15T10:30:00.000Z");
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

TEST_CASE("Test fetching history of node with a single integer nodeid entry", "[fetchopchistory]") {
  OpcUaTestServer server(4841);
  server.start();
  SingleProcessorTestController controller{minifi::test::utils::make_processor<processors::FetchOPCHistory>("FetchOPCHistory")};
  auto fetch_opc_processor = controller.getProcessor();
  REQUIRE(fetch_opc_processor->setProperty(processors::FetchOPCHistory::OPCServerEndPoint.name, "opc.tcp://127.0.0.1:4841/"));
  REQUIRE(fetch_opc_processor->setProperty(processors::FetchOPCHistory::NodeIDType.name, "Int"));
  REQUIRE(fetch_opc_processor->setProperty(processors::FetchOPCHistory::NodeID.name, "666"));
  REQUIRE(fetch_opc_processor->setProperty(processors::FetchOPCHistory::NameSpaceIndex.name, std::to_string(server.getNamespaceIndex())));

  const auto contains_modification_attributes = GENERATE(true, false);
  if (contains_modification_attributes) {
    REQUIRE(fetch_opc_processor->setProperty(processors::FetchOPCHistory::HistoryReadType.name, "Modified"));
  }

  const auto results = controller.trigger();
  REQUIRE(results.at(processors::FetchOPCHistory::Success).size() == 1);
  auto flow_file = results.at(processors::FetchOPCHistory::Success)[0];
  CHECK(controller.plan->getContent(flow_file) == "256");

  CHECK(flow_file->getAttribute("NodeID") == "666");
  CHECK(flow_file->getAttribute("NamespaceIndex") == std::to_string(server.getNamespaceIndex()));
  CHECK(flow_file->getAttribute("Sourcetimestamp") == "2001-01-01T22:22:00.000Z");
  if (contains_modification_attributes) {
    CHECK(flow_file->getAttribute("ModificationUsername") == "integer_user");
    CHECK(flow_file->getAttribute("ModificationUpdateType") == "Insert");
    CHECK(flow_file->getAttribute("ModificationTime") == "2001-01-01T22:22:00.000Z");
  } else {
    CHECK(flow_file->getAttribute("ModificationUsername") == std::nullopt);
    CHECK(flow_file->getAttribute("ModificationUpdateType") == std::nullopt);
    CHECK(flow_file->getAttribute("ModificationTime") == std::nullopt);
  }
}

TEST_CASE("Test fetching history after a specific timestamp", "[fetchopchistory]") {
  OpcUaTestServer server(4841);
  server.start();
  SingleProcessorTestController controller{minifi::test::utils::make_processor<processors::FetchOPCHistory>("FetchOPCHistory")};
  auto fetch_opc_processor = controller.getProcessor();
  REQUIRE(fetch_opc_processor->setProperty(processors::FetchOPCHistory::OPCServerEndPoint.name, "opc.tcp://127.0.0.1:4841/"));
  REQUIRE(fetch_opc_processor->setProperty(processors::FetchOPCHistory::NodeIDType.name, "String"));
  REQUIRE(fetch_opc_processor->setProperty(processors::FetchOPCHistory::NodeID.name, "INT2"));
  REQUIRE(fetch_opc_processor->setProperty(processors::FetchOPCHistory::NameSpaceIndex.name, std::to_string(server.getNamespaceIndex())));
  REQUIRE(fetch_opc_processor->setProperty(processors::FetchOPCHistory::StartTimestamp.name, "2025-10-01T00:00:00Z"));

  const auto contains_modification_attributes = GENERATE(true, false);
  if (contains_modification_attributes) {
    REQUIRE(fetch_opc_processor->setProperty(processors::FetchOPCHistory::HistoryReadType.name, "Modified"));
  }

  const auto results = controller.trigger();
  REQUIRE(results.at(processors::FetchOPCHistory::Success).size() == 2);
  auto flow_file = results.at(processors::FetchOPCHistory::Success)[0];
  CHECK(controller.plan->getContent(flow_file) == "3");
  CHECK(flow_file->getAttribute("NodeID") == "INT2");
  CHECK(flow_file->getAttribute("NamespaceIndex") == std::to_string(server.getNamespaceIndex()));
  CHECK(flow_file->getAttribute("Sourcetimestamp") == "2025-11-11T11:30:00.000Z");
  if (contains_modification_attributes) {
    CHECK(flow_file->getAttribute("ModificationUsername") == "admin_user");
    CHECK(flow_file->getAttribute("ModificationUpdateType") == "Update");
    CHECK(flow_file->getAttribute("ModificationTime") == "2025-11-11T11:30:00.000Z");
  } else {
    CHECK(flow_file->getAttribute("ModificationUsername") == std::nullopt);
    CHECK(flow_file->getAttribute("ModificationUpdateType") == std::nullopt);
    CHECK(flow_file->getAttribute("ModificationTime") == std::nullopt);
  }
  flow_file = results.at(processors::FetchOPCHistory::Success)[1];
  CHECK(controller.plan->getContent(flow_file) == "4");
  CHECK(flow_file->getAttribute("NodeID") == "INT2");
  CHECK(flow_file->getAttribute("Sourcetimestamp") == "2026-03-11T11:30:00.000Z");
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
  SingleProcessorTestController controller{minifi::test::utils::make_processor<processors::FetchOPCHistory>("FetchOPCHistory")};
  auto fetch_opc_processor = controller.getProcessor();
  REQUIRE(fetch_opc_processor->setProperty(processors::FetchOPCHistory::OPCServerEndPoint.name, "opc.tcp://127.0.0.1:4841/"));
  REQUIRE(fetch_opc_processor->setProperty(processors::FetchOPCHistory::NodeIDType.name, "String"));
  REQUIRE(fetch_opc_processor->setProperty(processors::FetchOPCHistory::NodeID.name, "INT2"));
  REQUIRE(fetch_opc_processor->setProperty(processors::FetchOPCHistory::NameSpaceIndex.name, std::to_string(server.getNamespaceIndex())));
  REQUIRE(fetch_opc_processor->setProperty(processors::FetchOPCHistory::EndTimestamp.name, "2025-11-12T00:00:00Z"));

  const auto contains_modification_attributes = GENERATE(true, false);
  if (contains_modification_attributes) {
    REQUIRE(fetch_opc_processor->setProperty(processors::FetchOPCHistory::HistoryReadType.name, "Modified"));
  }

  const auto results = controller.trigger();
  REQUIRE(results.at(processors::FetchOPCHistory::Success).size() == 2);
  auto flow_file = results.at(processors::FetchOPCHistory::Success)[0];
  CHECK(controller.plan->getContent(flow_file) == "2");
  CHECK(flow_file->getAttribute("NodeID") == "INT2");
  CHECK(flow_file->getAttribute("NamespaceIndex") == std::to_string(server.getNamespaceIndex()));
  CHECK(flow_file->getAttribute("Sourcetimestamp") == "2021-03-15T11:30:00.000Z");
  if (contains_modification_attributes) {
    CHECK(flow_file->getAttribute("ModificationUsername") == "admin_user");
    CHECK(flow_file->getAttribute("ModificationUpdateType") == "Insert");
    CHECK(flow_file->getAttribute("ModificationTime") == "2021-03-15T11:30:00.000Z");
  } else {
    CHECK(flow_file->getAttribute("ModificationUsername") == std::nullopt);
    CHECK(flow_file->getAttribute("ModificationUpdateType") == std::nullopt);
    CHECK(flow_file->getAttribute("ModificationTime") == std::nullopt);
  }
  flow_file = results.at(processors::FetchOPCHistory::Success)[1];
  CHECK(controller.plan->getContent(flow_file) == "3");
  CHECK(flow_file->getAttribute("NodeID") == "INT2");
  CHECK(flow_file->getAttribute("Sourcetimestamp") == "2025-11-11T11:30:00.000Z");
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
  OpcUaTestServer server(4841);
  server.start();
  SingleProcessorTestController controller{minifi::test::utils::make_processor<processors::FetchOPCHistory>("FetchOPCHistory")};
  auto fetch_opc_processor = controller.getProcessor();
  REQUIRE(fetch_opc_processor->setProperty(processors::FetchOPCHistory::OPCServerEndPoint.name, "opc.tcp://127.0.0.1:4841/"));
  REQUIRE(fetch_opc_processor->setProperty(processors::FetchOPCHistory::NodeIDType.name, "String"));
  REQUIRE(fetch_opc_processor->setProperty(processors::FetchOPCHistory::NodeID.name, "INT2"));
  REQUIRE(fetch_opc_processor->setProperty(processors::FetchOPCHistory::NameSpaceIndex.name, std::to_string(server.getNamespaceIndex())));
  REQUIRE(fetch_opc_processor->setProperty(processors::FetchOPCHistory::BatchSize.name, "2"));

  const auto contains_modification_attributes = GENERATE(true, false);
  if (contains_modification_attributes) {
    REQUIRE(fetch_opc_processor->setProperty(processors::FetchOPCHistory::HistoryReadType.name, "Modified"));
  }

  const auto results = controller.trigger();
  REQUIRE(results.at(processors::FetchOPCHistory::Success).size() == 2);
  auto flow_file = results.at(processors::FetchOPCHistory::Success)[0];
  CHECK(controller.plan->getContent(flow_file) == "2");
  CHECK(flow_file->getAttribute("NodeID") == "INT2");
  CHECK(flow_file->getAttribute("NamespaceIndex") == std::to_string(server.getNamespaceIndex()));
  CHECK(flow_file->getAttribute("Sourcetimestamp") == "2021-03-15T11:30:00.000Z");
  if (contains_modification_attributes) {
    CHECK(flow_file->getAttribute("ModificationUsername") == "admin_user");
    CHECK(flow_file->getAttribute("ModificationUpdateType") == "Insert");
    CHECK(flow_file->getAttribute("ModificationTime") == "2021-03-15T11:30:00.000Z");
  } else {
    CHECK(flow_file->getAttribute("ModificationUsername") == std::nullopt);
    CHECK(flow_file->getAttribute("ModificationUpdateType") == std::nullopt);
    CHECK(flow_file->getAttribute("ModificationTime") == std::nullopt);
  }
  flow_file = results.at(processors::FetchOPCHistory::Success)[1];
  CHECK(controller.plan->getContent(flow_file) == "3");
  CHECK(flow_file->getAttribute("NodeID") == "INT2");
  CHECK(flow_file->getAttribute("Sourcetimestamp") == "2025-11-11T11:30:00.000Z");
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

TEST_CASE("Test RecordSetWriter with JSON output format", "[fetchopchistory]") {
  OpcUaTestServer server(4841);
  server.start();
  SingleProcessorTestController controller{minifi::test::utils::make_processor<processors::FetchOPCHistory>("FetchOPCHistory")};
  auto json_record_set_writer = controller.plan->addController("JsonRecordSetWriter", "JsonRecordSetWriter");
  REQUIRE(controller.plan->setProperty(json_record_set_writer, "Output Grouping", "One Line Per Object"));
  auto fetch_opc_processor = controller.getProcessor();
  REQUIRE(fetch_opc_processor->setProperty(processors::FetchOPCHistory::OPCServerEndPoint.name, "opc.tcp://127.0.0.1:4841/"));
  REQUIRE(fetch_opc_processor->setProperty(processors::FetchOPCHistory::NodeIDType.name, "String"));
  REQUIRE(fetch_opc_processor->setProperty(processors::FetchOPCHistory::NodeID.name, "INT1"));
  REQUIRE(fetch_opc_processor->setProperty(processors::FetchOPCHistory::NameSpaceIndex.name, std::to_string(server.getNamespaceIndex())));
  REQUIRE(fetch_opc_processor->setProperty(processors::FetchOPCHistory::RecordSetWriter.name, "JsonRecordSetWriter"));

  std::string expected_json_content;
  SECTION("Fetch full history") {
    expected_json_content =
      R"({"Value":"1","Sourcetimestamp":"2024-06-15T10:30:00.000Z","NodeID":"INT1","NamespaceIndex":")" + std::to_string(server.getNamespaceIndex()) + "\","
      R"("ModificationUsername":"test_user","ModificationUpdateType":"Replace","ModificationTime":"2024-06-15T10:30:00.000Z"})";
    REQUIRE(fetch_opc_processor->setProperty(processors::FetchOPCHistory::HistoryReadType.name, "Modified"));
  }

  SECTION("Fetch raw history") {
    expected_json_content = R"({"Value":"1","Sourcetimestamp":"2024-06-15T10:30:00.000Z","NodeID":"INT1","NamespaceIndex":")" + std::to_string(server.getNamespaceIndex()) + "\"}";
  }

  const auto results = controller.trigger();
  verifyResults(controller, results, expected_json_content);
}

TEST_CASE("Test RecordSetWriter with JSON output format with multiple values", "[fetchopchistory]") {
  OpcUaTestServer server(4841);
  server.start();
  SingleProcessorTestController controller{minifi::test::utils::make_processor<processors::FetchOPCHistory>("FetchOPCHistory")};
  auto json_record_set_writer = controller.plan->addController("JsonRecordSetWriter", "JsonRecordSetWriter");
  auto fetch_opc_processor = controller.getProcessor();
  REQUIRE(fetch_opc_processor->setProperty(processors::FetchOPCHistory::OPCServerEndPoint.name, "opc.tcp://127.0.0.1:4841/"));
  REQUIRE(fetch_opc_processor->setProperty(processors::FetchOPCHistory::NodeIDType.name, "String"));
  REQUIRE(fetch_opc_processor->setProperty(processors::FetchOPCHistory::NodeID.name, "INT2"));
  REQUIRE(fetch_opc_processor->setProperty(processors::FetchOPCHistory::NameSpaceIndex.name, std::to_string(server.getNamespaceIndex())));
  REQUIRE(fetch_opc_processor->setProperty(processors::FetchOPCHistory::RecordSetWriter.name, "JsonRecordSetWriter"));

  std::string expected_json_content;
  SECTION("Fetch full history") {
    expected_json_content =
      R"([{"Value":"2","Sourcetimestamp":"2021-03-15T11:30:00.000Z","NodeID":"INT2","NamespaceIndex":")" + std::to_string(server.getNamespaceIndex()) + "\","
      R"("ModificationUsername":"admin_user","ModificationUpdateType":"Insert","ModificationTime":"2021-03-15T11:30:00.000Z"}, )"
      R"({"Value":"3","Sourcetimestamp":"2025-11-11T11:30:00.000Z","NodeID":"INT2","NamespaceIndex":")" + std::to_string(server.getNamespaceIndex()) + "\","
      R"("ModificationUsername":"admin_user","ModificationUpdateType":"Update","ModificationTime":"2025-11-11T11:30:00.000Z"}, )"
      R"({"Value":"4","Sourcetimestamp":"2026-03-11T11:30:00.000Z","NodeID":"INT2","NamespaceIndex":")" + std::to_string(server.getNamespaceIndex()) + "\","
      R"("ModificationUsername":"test_user","ModificationUpdateType":"Replace","ModificationTime":"2026-03-11T11:30:00.000Z"}])";
    REQUIRE(fetch_opc_processor->setProperty(processors::FetchOPCHistory::HistoryReadType.name, "Modified"));
  }

  SECTION("Fetch raw history") {
    expected_json_content =
      R"([{"Value":"2","Sourcetimestamp":"2021-03-15T11:30:00.000Z","NodeID":"INT2","NamespaceIndex":")" + std::to_string(server.getNamespaceIndex()) + "\"},"
      R"({"Value":"3","Sourcetimestamp":"2025-11-11T11:30:00.000Z","NodeID":"INT2","NamespaceIndex":")" + std::to_string(server.getNamespaceIndex()) + "\"},"
      R"({"Value":"4","Sourcetimestamp":"2026-03-11T11:30:00.000Z","NodeID":"INT2","NamespaceIndex":")" + std::to_string(server.getNamespaceIndex()) + "\"}]";
  }

  const auto results = controller.trigger();
  verifyResults(controller, results, expected_json_content);
}

TEST_CASE("Test multiple triggers with state kept in state manager", "[fetchopchistory]") {
  OpcUaTestServer server(4841);
  server.start();
  SingleProcessorTestController controller{minifi::test::utils::make_processor<processors::FetchOPCHistory>("FetchOPCHistory")};
  auto fetch_opc_processor = controller.getProcessor();
  REQUIRE(fetch_opc_processor->setProperty(processors::FetchOPCHistory::OPCServerEndPoint.name, "opc.tcp://127.0.0.1:4841/"));
  REQUIRE(fetch_opc_processor->setProperty(processors::FetchOPCHistory::NodeIDType.name, "String"));
  REQUIRE(fetch_opc_processor->setProperty(processors::FetchOPCHistory::NodeID.name, "INT2"));
  REQUIRE(fetch_opc_processor->setProperty(processors::FetchOPCHistory::NameSpaceIndex.name, std::to_string(server.getNamespaceIndex())));
  REQUIRE(fetch_opc_processor->setProperty(processors::FetchOPCHistory::BatchSize.name, "1"));

  const auto contains_modification_attributes = GENERATE(true, false);
  if (contains_modification_attributes) {
    REQUIRE(fetch_opc_processor->setProperty(processors::FetchOPCHistory::HistoryReadType.name, "Modified"));
  }

  auto results = controller.trigger();
  REQUIRE(results.at(processors::FetchOPCHistory::Success).size() == 1);
  auto flow_file = results.at(processors::FetchOPCHistory::Success)[0];
  CHECK(controller.plan->getContent(flow_file) == "2");
  CHECK(flow_file->getAttribute("NodeID") == "INT2");
  CHECK(flow_file->getAttribute("NamespaceIndex") == std::to_string(server.getNamespaceIndex()));
  CHECK(flow_file->getAttribute("Sourcetimestamp") == "2021-03-15T11:30:00.000Z");
  if (contains_modification_attributes) {
    CHECK(flow_file->getAttribute("ModificationUsername") == "admin_user");
    CHECK(flow_file->getAttribute("ModificationUpdateType") == "Insert");
    CHECK(flow_file->getAttribute("ModificationTime") == "2021-03-15T11:30:00.000Z");
  } else {
    CHECK(flow_file->getAttribute("ModificationUsername") == std::nullopt);
    CHECK(flow_file->getAttribute("ModificationUpdateType") == std::nullopt);
    CHECK(flow_file->getAttribute("ModificationTime") == std::nullopt);
  }

  results = controller.trigger();
  REQUIRE(results.at(processors::FetchOPCHistory::Success).size() == 1);
  flow_file = results.at(processors::FetchOPCHistory::Success)[0];
  CHECK(controller.plan->getContent(flow_file) == "3");
  CHECK(flow_file->getAttribute("NodeID") == "INT2");
  CHECK(flow_file->getAttribute("NamespaceIndex") == std::to_string(server.getNamespaceIndex()));
  CHECK(flow_file->getAttribute("Sourcetimestamp") == "2025-11-11T11:30:00.000Z");
  if (contains_modification_attributes) {
    CHECK(flow_file->getAttribute("ModificationUsername") == "admin_user");
    CHECK(flow_file->getAttribute("ModificationUpdateType") == "Update");
    CHECK(flow_file->getAttribute("ModificationTime") == "2025-11-11T11:30:00.000Z");
  } else {
    CHECK(flow_file->getAttribute("ModificationUsername") == std::nullopt);
    CHECK(flow_file->getAttribute("ModificationUpdateType") == std::nullopt);
    CHECK(flow_file->getAttribute("ModificationTime") == std::nullopt);
  }

  results = controller.trigger();
  REQUIRE(results.at(processors::FetchOPCHistory::Success).size() == 1);
  flow_file = results.at(processors::FetchOPCHistory::Success)[0];
  CHECK(controller.plan->getContent(flow_file) == "4");
  CHECK(flow_file->getAttribute("NodeID") == "INT2");
  CHECK(flow_file->getAttribute("NamespaceIndex") == std::to_string(server.getNamespaceIndex()));
  CHECK(flow_file->getAttribute("Sourcetimestamp") == "2026-03-11T11:30:00.000Z");
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

}  // namespace org::apache::nifi::minifi::test
