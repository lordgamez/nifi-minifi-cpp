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

TEST_CASE("Test namespace cannot be empty", "[putopcprocessor]") {
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
  put_opc_processor->setProperty(processors::PutOPCProcessor::TargetNodeNameSpaceIndex, "${missing}");

  const auto results = controller.trigger("42");
  REQUIRE(results.at(processors::PutOPCProcessor::Success).empty());
  REQUIRE(results.at(processors::PutOPCProcessor::Failure).size() == 1);
  auto flow_file = results.at(processors::PutOPCProcessor::Failure)[0];
  CHECK(controller.plan->getContent(flow_file) == "42");
  REQUIRE(LogTestController::getInstance().contains("had no target namespace index specified, routing to failure"));
}

TEST_CASE("Test valid namespace being required", "[putopcprocessor]") {
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
  put_opc_processor->setProperty(processors::PutOPCProcessor::TargetNodeNameSpaceIndex, "invalid_index");

  const auto results = controller.trigger("42");
  REQUIRE(results.at(processors::PutOPCProcessor::Success).empty());
  REQUIRE(results.at(processors::PutOPCProcessor::Failure).size() == 1);
  auto flow_file = results.at(processors::PutOPCProcessor::Failure)[0];
  CHECK(controller.plan->getContent(flow_file) == "42");
  REQUIRE(LogTestController::getInstance().contains("has invalid namespace index (invalid_index), routing to failure"));
}

TEST_CASE("Test username and password should both be provided", "[putopcprocessor]") {
  SingleProcessorTestController controller{std::make_unique<processors::PutOPCProcessor>("PutOPCProcessor")};
  auto put_opc_processor = controller.getProcessor();
  put_opc_processor->setProperty(processors::PutOPCProcessor::OPCServerEndPoint, "opc.tcp://127.0.0.1:4840/");
  put_opc_processor->setProperty(processors::PutOPCProcessor::Username, "user");
  put_opc_processor->setProperty(processors::PutOPCProcessor::Password, "");

  REQUIRE_THROWS_WITH(controller.trigger("42"), "Process Schedule Operation: Both or neither of Username and Password should be provided!");
}

TEST_CASE("Test certificate path and key path should both be provided", "[putopcprocessor]") {
  SingleProcessorTestController controller{std::make_unique<processors::PutOPCProcessor>("PutOPCProcessor")};
  auto put_opc_processor = controller.getProcessor();
  put_opc_processor->setProperty(processors::PutOPCProcessor::OPCServerEndPoint, "opc.tcp://127.0.0.1:4840/");
  put_opc_processor->setProperty(processors::PutOPCProcessor::CertificatePath, "cert");
  put_opc_processor->setProperty(processors::PutOPCProcessor::KeyPath, "");

  REQUIRE_THROWS_WITH(controller.trigger("42"), "Process Schedule Operation: All or none of Certificate path and Key path should be provided!");
}

TEST_CASE("Test application uri should be provided if certificate is provided", "[putopcprocessor]") {
  SingleProcessorTestController controller{std::make_unique<processors::PutOPCProcessor>("PutOPCProcessor")};
  auto put_opc_processor = controller.getProcessor();
  put_opc_processor->setProperty(processors::PutOPCProcessor::OPCServerEndPoint, "opc.tcp://127.0.0.1:4840/");
  put_opc_processor->setProperty(processors::PutOPCProcessor::CertificatePath, "cert");
  put_opc_processor->setProperty(processors::PutOPCProcessor::KeyPath, "key");

  REQUIRE_THROWS_WITH(controller.trigger("42"), "Process Schedule Operation: Application URI must be provided if Certificate path is provided!");
}

TEST_CASE("Test certificate path must be valid", "[putopcprocessor]") {
  SingleProcessorTestController controller{std::make_unique<processors::PutOPCProcessor>("PutOPCProcessor")};
  auto put_opc_processor = controller.getProcessor();
  put_opc_processor->setProperty(processors::PutOPCProcessor::OPCServerEndPoint, "opc.tcp://127.0.0.1:4840/");
  put_opc_processor->setProperty(processors::PutOPCProcessor::CertificatePath, "/invalid/cert/path");
  put_opc_processor->setProperty(processors::PutOPCProcessor::KeyPath, "key");
  put_opc_processor->setProperty(processors::PutOPCProcessor::ApplicationURI, "appuri");

  REQUIRE_THROWS_WITH(controller.trigger("42"), "Process Schedule Operation: Failed to load cert from path: /invalid/cert/path");
}

TEST_CASE("Test key path must be valid", "[putopcprocessor]") {
  SingleProcessorTestController controller{std::make_unique<processors::PutOPCProcessor>("PutOPCProcessor")};
  auto put_opc_processor = controller.getProcessor();
  auto test_cert_path = controller.createTempDirectory() /  "test_cert.pem";
  {
    std::ofstream cert_file(test_cert_path);
    cert_file << "test";
    cert_file.close();
  }
  put_opc_processor->setProperty(processors::PutOPCProcessor::OPCServerEndPoint, "opc.tcp://127.0.0.1:4840/");
  put_opc_processor->setProperty(processors::PutOPCProcessor::CertificatePath, test_cert_path.string());
  put_opc_processor->setProperty(processors::PutOPCProcessor::KeyPath, "/invalid/key");
  put_opc_processor->setProperty(processors::PutOPCProcessor::ApplicationURI, "appuri");

  REQUIRE_THROWS_WITH(controller.trigger("42"), "Process Schedule Operation: Failed to load key from path: /invalid/key");
}

TEST_CASE("Test trusted certs path must be valid", "[putopcprocessor]") {
  SingleProcessorTestController controller{std::make_unique<processors::PutOPCProcessor>("PutOPCProcessor")};
  auto put_opc_processor = controller.getProcessor();
  auto test_cert_path = controller.createTempDirectory() /  "test_cert.pem";
  {
    std::ofstream cert_file(test_cert_path);
    cert_file << "test";
    cert_file.close();
  }
  put_opc_processor->setProperty(processors::PutOPCProcessor::OPCServerEndPoint, "opc.tcp://127.0.0.1:4840/");
  put_opc_processor->setProperty(processors::PutOPCProcessor::CertificatePath, test_cert_path.string());
  put_opc_processor->setProperty(processors::PutOPCProcessor::KeyPath, test_cert_path.string());
  put_opc_processor->setProperty(processors::PutOPCProcessor::TrustedPath, "/invalid/trusted");
  put_opc_processor->setProperty(processors::PutOPCProcessor::ApplicationURI, "appuri");

  REQUIRE_THROWS_WITH(controller.trigger("42"), "Process Schedule Operation: Failed to load trusted server certs from path: /invalid/trusted");
}

TEST_CASE("Test invalid int node id", "[putopcprocessor]") {
  SingleProcessorTestController controller{std::make_unique<processors::PutOPCProcessor>("PutOPCProcessor")};
  auto put_opc_processor = controller.getProcessor();
  put_opc_processor->setProperty(processors::PutOPCProcessor::OPCServerEndPoint, "opc.tcp://127.0.0.1:4840/");
  put_opc_processor->setProperty(processors::PutOPCProcessor::ParentNodeIDType, "Int");
  put_opc_processor->setProperty(processors::PutOPCProcessor::ParentNodeID, "Simulator/Default/Device1");
  put_opc_processor->setProperty(processors::PutOPCProcessor::ValueType, "Int32");

  REQUIRE_THROWS_WITH(controller.trigger("42"), "Process Schedule Operation: Simulator/Default/Device1 cannot be used as an int type node ID");
}

TEST_CASE("Test invalid parent node id path", "[putopcprocessor]") {
  OpcUaTestServer server;
  server.start();
  SingleProcessorTestController controller{std::make_unique<processors::PutOPCProcessor>("PutOPCProcessor")};
  auto put_opc_processor = controller.getProcessor();
  put_opc_processor->setProperty(processors::PutOPCProcessor::OPCServerEndPoint, "opc.tcp://127.0.0.1:4840/");
  put_opc_processor->setProperty(processors::PutOPCProcessor::ParentNodeIDType, "Path");
  put_opc_processor->setProperty(processors::PutOPCProcessor::ParentNodeID, "Simulator/Default/Device1/INT99");
  put_opc_processor->setProperty(processors::PutOPCProcessor::ParentNameSpaceIndex, std::to_string(server.getNamespaceIndex()));
  put_opc_processor->setProperty(processors::PutOPCProcessor::ValueType, "Int32");
  put_opc_processor->setProperty(processors::PutOPCProcessor::TargetNodeIDType, "Int");
  put_opc_processor->setProperty(processors::PutOPCProcessor::TargetNodeID, "9999");
  put_opc_processor->setProperty(processors::PutOPCProcessor::TargetNodeNameSpaceIndex, "2");
  put_opc_processor->setProperty(processors::PutOPCProcessor::TargetNodeBrowseName, "everything");

  const auto results = controller.trigger("42");
  REQUIRE(results.at(processors::PutOPCProcessor::Success).empty());
  REQUIRE(results.at(processors::PutOPCProcessor::Failure).empty());
  REQUIRE(LogTestController::getInstance().contains("to node id, no flow files will be put"));
}

TEST_CASE("Test missing target node id", "[putopcprocessor]") {
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
  put_opc_processor->setProperty(processors::PutOPCProcessor::TargetNodeID, "${missing}");
  put_opc_processor->setProperty(processors::PutOPCProcessor::TargetNodeNameSpaceIndex, "2");
  put_opc_processor->setProperty(processors::PutOPCProcessor::TargetNodeBrowseName, "everything");

  const auto results = controller.trigger("42");
  REQUIRE(results.at(processors::PutOPCProcessor::Success).empty());
  REQUIRE(results.at(processors::PutOPCProcessor::Failure).size() == 1);
  auto flow_file = results.at(processors::PutOPCProcessor::Failure)[0];
  CHECK(controller.plan->getContent(flow_file) == "42");
  REQUIRE(LogTestController::getInstance().contains("had target node ID type specified (Int) without ID, routing to failure"));
}

TEST_CASE("Test invalid target node id", "[putopcprocessor]") {
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
  put_opc_processor->setProperty(processors::PutOPCProcessor::TargetNodeID, "invalid_int");
  put_opc_processor->setProperty(processors::PutOPCProcessor::TargetNodeNameSpaceIndex, "2");
  put_opc_processor->setProperty(processors::PutOPCProcessor::TargetNodeBrowseName, "everything");

  const auto results = controller.trigger("42");
  REQUIRE(results.at(processors::PutOPCProcessor::Success).empty());
  REQUIRE(results.at(processors::PutOPCProcessor::Failure).size() == 1);
  auto flow_file = results.at(processors::PutOPCProcessor::Failure)[0];
  CHECK(controller.plan->getContent(flow_file) == "42");
  REQUIRE(LogTestController::getInstance().contains("target node ID is not a valid integer: invalid_int. Routing to failure"));
}

TEST_CASE("Test missing target node type", "[putopcprocessor]") {
  OpcUaTestServer server;
  server.start();
  SingleProcessorTestController controller{std::make_unique<processors::PutOPCProcessor>("PutOPCProcessor")};
  auto put_opc_processor = controller.getProcessor();
  put_opc_processor->setProperty(processors::PutOPCProcessor::OPCServerEndPoint, "opc.tcp://127.0.0.1:4840/");
  put_opc_processor->setProperty(processors::PutOPCProcessor::ParentNodeIDType, "Path");
  put_opc_processor->setProperty(processors::PutOPCProcessor::ParentNodeID, "Simulator/Default/Device1");
  put_opc_processor->setProperty(processors::PutOPCProcessor::ParentNameSpaceIndex, std::to_string(server.getNamespaceIndex()));
  put_opc_processor->setProperty(processors::PutOPCProcessor::ValueType, "Int32");
  put_opc_processor->setProperty(processors::PutOPCProcessor::TargetNodeIDType, "${missing}");
  put_opc_processor->setProperty(processors::PutOPCProcessor::TargetNodeID, "9999");
  put_opc_processor->setProperty(processors::PutOPCProcessor::TargetNodeNameSpaceIndex, "2");
  put_opc_processor->setProperty(processors::PutOPCProcessor::TargetNodeBrowseName, "everything");

  const auto results = controller.trigger("42", {{"invalid_type", "invalid"}});
  REQUIRE(results.at(processors::PutOPCProcessor::Success).empty());
  REQUIRE(results.at(processors::PutOPCProcessor::Failure).size() == 1);
  auto flow_file = results.at(processors::PutOPCProcessor::Failure)[0];
  CHECK(controller.plan->getContent(flow_file) == "42");
  REQUIRE(LogTestController::getInstance().contains("has invalid target node id type (), routing to failure"));
}

TEST_CASE("Test invalid target node type", "[putopcprocessor]") {
  OpcUaTestServer server;
  server.start();
  SingleProcessorTestController controller{std::make_unique<processors::PutOPCProcessor>("PutOPCProcessor")};
  auto put_opc_processor = controller.getProcessor();
  put_opc_processor->setProperty(processors::PutOPCProcessor::OPCServerEndPoint, "opc.tcp://127.0.0.1:4840/");
  put_opc_processor->setProperty(processors::PutOPCProcessor::ParentNodeIDType, "Path");
  put_opc_processor->setProperty(processors::PutOPCProcessor::ParentNodeID, "Simulator/Default/Device1");
  put_opc_processor->setProperty(processors::PutOPCProcessor::ParentNameSpaceIndex, std::to_string(server.getNamespaceIndex()));
  put_opc_processor->setProperty(processors::PutOPCProcessor::ValueType, "Int32");
  put_opc_processor->setProperty(processors::PutOPCProcessor::TargetNodeIDType, "${invalid_type}");
  put_opc_processor->setProperty(processors::PutOPCProcessor::TargetNodeID, "9999");
  put_opc_processor->setProperty(processors::PutOPCProcessor::TargetNodeNameSpaceIndex, "2");
  put_opc_processor->setProperty(processors::PutOPCProcessor::TargetNodeBrowseName, "everything");

  const auto results = controller.trigger("42", {{"invalid_type", "invalid"}});
  REQUIRE(results.at(processors::PutOPCProcessor::Success).empty());
  REQUIRE(results.at(processors::PutOPCProcessor::Failure).size() == 1);
  auto flow_file = results.at(processors::PutOPCProcessor::Failure)[0];
  CHECK(controller.plan->getContent(flow_file) == "42");
  REQUIRE(LogTestController::getInstance().contains("target node ID type is invalid: invalid. Routing to failure"));
}

TEST_CASE("Test value type mismatch", "[putopcprocessor]") {
  OpcUaTestServer server;
  server.start();
  SingleProcessorTestController controller{std::make_unique<processors::PutOPCProcessor>("PutOPCProcessor")};
  auto put_opc_processor = controller.getProcessor();
  put_opc_processor->setProperty(processors::PutOPCProcessor::OPCServerEndPoint, "opc.tcp://127.0.0.1:4840/");
  put_opc_processor->setProperty(processors::PutOPCProcessor::ParentNodeIDType, "Path");
  put_opc_processor->setProperty(processors::PutOPCProcessor::ParentNodeID, "Simulator/Default/Device1");
  put_opc_processor->setProperty(processors::PutOPCProcessor::ParentNameSpaceIndex, std::to_string(server.getNamespaceIndex()));
  put_opc_processor->setProperty(processors::PutOPCProcessor::ValueType, "Boolean");
  put_opc_processor->setProperty(processors::PutOPCProcessor::TargetNodeIDType, "Int");
  put_opc_processor->setProperty(processors::PutOPCProcessor::TargetNodeID, "9999");
  put_opc_processor->setProperty(processors::PutOPCProcessor::TargetNodeNameSpaceIndex, "2");
  put_opc_processor->setProperty(processors::PutOPCProcessor::TargetNodeBrowseName, "everything");

  const auto results = controller.trigger("42");
  REQUIRE(results.at(processors::PutOPCProcessor::Success).empty());
  REQUIRE(results.at(processors::PutOPCProcessor::Failure).size() == 1);
  auto flow_file = results.at(processors::PutOPCProcessor::Failure)[0];
  CHECK(controller.plan->getContent(flow_file) == "42");
  REQUIRE(LogTestController::getInstance().contains("Failed to convert 42 to data type Boolean"));
}

}  // namespace org::apache::nifi::minifi::test
