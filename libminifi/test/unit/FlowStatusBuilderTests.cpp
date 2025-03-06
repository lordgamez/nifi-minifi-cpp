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
#include <unordered_set>

#include "unit/TestBase.h"
#include "unit/Catch.h"
#include "c2/FlowStatusBuilder.h"
#include "unit/DummyProcessor.h"
#include "core/ProcessorNode.h"
#include "core/BulletinStore.h"
#include "properties/Configure.h"

namespace org::apache::nifi::minifi::test {

TEST_CASE("Parse invalid flow status query string", "[flowstatusbuilder]") {
  REQUIRE_THROWS_WITH(c2::FlowStatusRequest("invalid query string"), "Invalid query string: invalid query string");
}

TEST_CASE("Parse invalid flow status query type", "[flowstatusbuilder]") {
  REQUIRE_THROWS_WITH(c2::FlowStatusRequest("invalid_type:TaiFile:health"), "Invalid query type: invalid_type");
}

TEST_CASE("Parse two part flow status query", "[flowstatusbuilder]") {
  c2::FlowStatusRequest request("processor:health,status");
  CHECK(request.query_type == c2::FlowStatusQueryType::processor);
  CHECK(request.identifier.empty());
  CHECK(request.options == std::unordered_set<std::string>{"health", "status"});
}

TEST_CASE("Parse three part flow status query", "[flowstatusbuilder]") {
  c2::FlowStatusRequest request("processor:TailFile:health");
  CHECK(request.query_type == c2::FlowStatusQueryType::processor);
  CHECK(request.identifier == "TailFile");
  CHECK(request.options == std::unordered_set<std::string>{"health"});
}

TEST_CASE("Build empty flow status", "[flowstatusbuilder]") {
  c2::FlowStatusBuilder flow_status_builder;
  auto status = flow_status_builder.buildFlowStatus({});
  CHECK(status["controllerServiceStatusList"].IsNull());
  CHECK(status["connectionStatusList"].IsNull());
  CHECK(status["remoteProcessGroupStatusList"].IsNull());
  CHECK(status["instanceStatus"].IsNull());
  CHECK(status["systemDiagnosticsStatus"].IsNull());
  CHECK(status["processorStatusList"].IsNull());
  CHECK(status["errorsGeneratingReport"].Empty());
}

TEST_CASE("Build health status for single processor", "[flowstatusbuilder]") {
  c2::FlowStatusBuilder flow_status_builder;
  core::ProcessGroup process_group(core::ROOT_PROCESS_GROUP, "root");
  auto processor = std::make_unique<DummyProcessor>("DummyProcessor", minifi::utils::Identifier::parse("4d7fa7e6-2459-46dd-b2ba-61517239edf5").value());
  process_group.addProcessor(std::move(processor));
  flow_status_builder.setRoot(&process_group);
  auto status = flow_status_builder.buildFlowStatus({c2::FlowStatusRequest{"processor:DummyProcessor:health"}});
  REQUIRE(status["processorStatusList"].GetArray().Size() == 1);
  CHECK(status["processorStatusList"].GetArray()[0]["id"] == "4d7fa7e6-2459-46dd-b2ba-61517239edf5");
  CHECK(status["processorStatusList"].GetArray()[0]["name"] == "DummyProcessor");
  CHECK(status["processorStatusList"].GetArray()[0]["processorStats"].IsNull());
  CHECK(status["processorStatusList"].GetArray()[0]["bulletinList"].IsNull());
  CHECK(status["processorStatusList"].GetArray()[0]["processorHealth"]["runStatus"] == "Stopped");
  CHECK_FALSE(status["processorStatusList"].GetArray()[0]["processorHealth"]["hasBulletins"].GetBool());
  CHECK(status["errorsGeneratingReport"].Empty());
}

TEST_CASE("Build stats for single processor", "[flowstatusbuilder]") {
  c2::FlowStatusBuilder flow_status_builder;
  core::ProcessGroup process_group(core::ROOT_PROCESS_GROUP, "root");
  auto processor = std::make_unique<DummyProcessor>("DummyProcessor", minifi::utils::Identifier::parse("4d7fa7e6-2459-46dd-b2ba-61517239edf5").value());
  processor->getMetrics()->invocations() = 1;
  processor->getMetrics()->incomingFlowFiles() = 2;
  processor->getMetrics()->bytesRead() = 3;
  processor->getMetrics()->bytesWritten() = 4;
  processor->getMetrics()->transferredFlowFiles() = 5;
  processor->getMetrics()->processingNanos() = 6;
  processor->getMetrics()->incomingBytes() = 7;
  processor->getMetrics()->transferredBytes() = 8;
  process_group.addProcessor(std::move(processor));
  flow_status_builder.setRoot(&process_group);
  auto status = flow_status_builder.buildFlowStatus({c2::FlowStatusRequest{"processor:DummyProcessor:stats"}});
  REQUIRE(status["processorStatusList"].GetArray().Size() == 1);
  CHECK(status["processorStatusList"].GetArray()[0]["id"] == "4d7fa7e6-2459-46dd-b2ba-61517239edf5");
  CHECK(status["processorStatusList"].GetArray()[0]["name"] == "DummyProcessor");
  CHECK(status["processorStatusList"].GetArray()[0]["bulletinList"].IsNull());
  CHECK(status["processorStatusList"].GetArray()[0]["processorHealth"].IsNull());
  CHECK(status["processorStatusList"].GetArray()[0]["processorStats"]["flowfilesReceived"].GetInt64() == 2);
  CHECK(status["processorStatusList"].GetArray()[0]["processorStats"]["bytesRead"].GetInt64() == 3);
  CHECK(status["processorStatusList"].GetArray()[0]["processorStats"]["bytesWritten"].GetInt64() == 4);
  CHECK(status["processorStatusList"].GetArray()[0]["processorStats"]["flowfilesSent"].GetInt64() == 5);
  CHECK(status["processorStatusList"].GetArray()[0]["processorStats"]["invocations"].GetInt64() == 1);
  CHECK(status["processorStatusList"].GetArray()[0]["processorStats"]["processingNanos"].GetInt64() == 6);
  CHECK(status["processorStatusList"].GetArray()[0]["processorStats"]["incomingBytes"].GetInt64() == 7);
  CHECK(status["processorStatusList"].GetArray()[0]["processorStats"]["transferredBytes"].GetInt64() == 8);
  CHECK(status["errorsGeneratingReport"].Empty());
}

TEST_CASE("Build bulletins for single processor", "[flowstatusbuilder]") {
  c2::FlowStatusBuilder flow_status_builder;
  core::ProcessGroup process_group(core::ROOT_PROCESS_GROUP, "root");
  auto processor = std::make_unique<DummyProcessor>("DummyProcessor", minifi::utils::Identifier::parse("4d7fa7e6-2459-46dd-b2ba-61517239edf5").value());
  auto processor_ptr = processor.get();
  process_group.addProcessor(std::move(processor));
  flow_status_builder.setRoot(&process_group);
  auto conf = std::make_shared<minifi::ConfigureImpl>();
  auto now = std::chrono::system_clock::now();
  auto unix_timestamp = std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count();
  core::BulletinStore bulletin_store(*conf);
  bulletin_store.addProcessorBulletin(*processor_ptr, core::logging::LOG_LEVEL::err, "error message");
  bulletin_store.addProcessorBulletin(*processor_ptr, core::logging::LOG_LEVEL::critical, "critical message");
  flow_status_builder.setBulletinStore(&bulletin_store);
  auto status = flow_status_builder.buildFlowStatus({c2::FlowStatusRequest{"processor:DummyProcessor:health,bulletins"}});
  REQUIRE(status["processorStatusList"].GetArray().Size() == 1);
  CHECK(status["processorStatusList"].GetArray()[0]["id"] == "4d7fa7e6-2459-46dd-b2ba-61517239edf5");
  CHECK(status["processorStatusList"].GetArray()[0]["name"] == "DummyProcessor");
  CHECK(status["processorStatusList"].GetArray()[0]["processorStats"].IsNull());
  CHECK(status["processorStatusList"].GetArray()[0]["processorHealth"]["hasBulletins"].GetBool());
  auto bulletin_array = status["processorStatusList"].GetArray()[0]["bulletinList"].GetArray();
  CHECK(bulletin_array.Size() == 2);
  CHECK(bulletin_array[0]["timestamp"].GetInt64() >= unix_timestamp);
  CHECK(bulletin_array[0]["message"].GetString() == std::string{"error message"});
  CHECK(bulletin_array[1]["timestamp"].GetInt64() >= unix_timestamp);
  CHECK(bulletin_array[1]["message"].GetString() == std::string{"critical message"});
  CHECK(status["errorsGeneratingReport"].Empty());
}

TEST_CASE("Build health status for all processors", "[flowstatusbuilder]") {
  c2::FlowStatusBuilder flow_status_builder;
  core::ProcessGroup process_group(core::ROOT_PROCESS_GROUP, "root");
  auto processor1 = std::make_unique<DummyProcessor>("DummyProcessor1", minifi::utils::Identifier::parse("123fa7e6-2459-46dd-b2ba-61517239edf5").value());
  auto processor2 = std::make_unique<DummyProcessor>("DummyProcessor2", minifi::utils::Identifier::parse("456fa7e6-2459-46dd-b2ba-61517239edf5").value());
  process_group.addProcessor(std::move(processor1));
  process_group.addProcessor(std::move(processor2));
  flow_status_builder.setRoot(&process_group);
  auto status = flow_status_builder.buildFlowStatus({c2::FlowStatusRequest{"processor:all:health"}});
  REQUIRE(status["processorStatusList"].GetArray().Size() == 2);
  std::unordered_set<std::string> expected_processor_ids = {"123fa7e6-2459-46dd-b2ba-61517239edf5", "456fa7e6-2459-46dd-b2ba-61517239edf5"};
  std::unordered_set<std::string> expected_processor_names = {"DummyProcessor1", "DummyProcessor2"};
  for (const auto& processor_status : status["processorStatusList"].GetArray()) {
    auto id = processor_status["id"].GetString();
    auto name = processor_status["name"].GetString();
    CHECK(expected_processor_ids.contains(id));
    CHECK(expected_processor_names.contains(name));
    expected_processor_ids.erase(id);
    expected_processor_names.erase(name);
  }
  CHECK(status["errorsGeneratingReport"].Empty());
}

TEST_CASE("Non-existent processor generates an error", "[flowstatusbuilder]") {
  c2::FlowStatusBuilder flow_status_builder;
  core::ProcessGroup process_group(core::ROOT_PROCESS_GROUP, "root");
  auto processor = std::make_unique<DummyProcessor>("DummyProcessor", minifi::utils::Identifier::parse("4d7fa7e6-2459-46dd-b2ba-61517239edf5").value());
  process_group.addProcessor(std::move(processor));
  flow_status_builder.setRoot(&process_group);
  auto status = flow_status_builder.buildFlowStatus({c2::FlowStatusRequest{"processor:InvalidProcessor:health"}});
  CHECK(status["controllerServiceStatusList"].IsNull());
  CHECK(status["connectionStatusList"].IsNull());
  CHECK(status["remoteProcessGroupStatusList"].IsNull());
  CHECK(status["instanceStatus"].IsNull());
  CHECK(status["systemDiagnosticsStatus"].IsNull());
  CHECK(status["processorStatusList"].Empty());
  CHECK(status["errorsGeneratingReport"].GetArray().Size() == 1);
  CHECK(status["errorsGeneratingReport"].GetArray()[0].GetString() == std::string{"Unable to get processorStatus: No processor with key 'InvalidProcessor' to report status on"});
}

TEST_CASE("Build processor status with only invalid options", "[flowstatusbuilder]") {
  c2::FlowStatusBuilder flow_status_builder;
  core::ProcessGroup process_group(core::ROOT_PROCESS_GROUP, "root");
  auto processor = std::make_unique<DummyProcessor>("DummyProcessor", minifi::utils::Identifier::parse("4d7fa7e6-2459-46dd-b2ba-61517239edf5").value());
  process_group.addProcessor(std::move(processor));
  flow_status_builder.setRoot(&process_group);
  auto status = flow_status_builder.buildFlowStatus({c2::FlowStatusRequest{"processor:DummyProcessor:invalid1,invalid2"}});
  REQUIRE(status["processorStatusList"].GetArray().Size() == 1);
  CHECK(status["processorStatusList"].GetArray()[0]["id"] == "4d7fa7e6-2459-46dd-b2ba-61517239edf5");
  CHECK(status["processorStatusList"].GetArray()[0]["name"] == "DummyProcessor");
  CHECK(status["processorStatusList"].GetArray()[0]["bulletinList"].IsNull());
  CHECK(status["processorStatusList"].GetArray()[0]["processorHealth"].IsNull());
  CHECK(status["processorStatusList"].GetArray()[0]["processorStats"].IsNull());
  CHECK(status["errorsGeneratingReport"].Empty());
}

TEST_CASE("Building processor status fails with incomplete query", "[flowstatusbuilder]") {
  c2::FlowStatusBuilder flow_status_builder;
  core::ProcessGroup process_group(core::ROOT_PROCESS_GROUP, "root");
  auto processor = std::make_unique<DummyProcessor>("DummyProcessor", minifi::utils::Identifier::parse("4d7fa7e6-2459-46dd-b2ba-61517239edf5").value());
  process_group.addProcessor(std::move(processor));
  flow_status_builder.setRoot(&process_group);
  auto status = flow_status_builder.buildFlowStatus({c2::FlowStatusRequest{"processor:DummyProcessor"}});
  CHECK(status["controllerServiceStatusList"].IsNull());
  CHECK(status["connectionStatusList"].IsNull());
  CHECK(status["remoteProcessGroupStatusList"].IsNull());
  CHECK(status["instanceStatus"].IsNull());
  CHECK(status["systemDiagnosticsStatus"].IsNull());
  CHECK(status["processorStatusList"].Empty());
  CHECK(status["errorsGeneratingReport"].GetArray().Size() == 1);
  CHECK(status["errorsGeneratingReport"].GetArray()[0].GetString() == std::string{"Unable to get processorStatus: Query is incomplete"});
}

TEST_CASE("Build health status for single connection", "[flowstatusbuilder]") {
  c2::FlowStatusBuilder flow_status_builder;
  core::ProcessGroup process_group(core::ROOT_PROCESS_GROUP, "root");
  auto connection = std::make_unique<ConnectionImpl>(nullptr, nullptr, "Conn1", minifi::utils::Identifier::parse("123fa7e6-2459-46dd-b2ba-61517239edf5").value());
  std::vector<std::shared_ptr<core::FlowFile>> flow_files{std::make_shared<core::FlowFileImpl>()};
  connection->multiPut(flow_files);
  process_group.addConnection(std::move(connection));
  flow_status_builder.setRoot(&process_group);
  auto status = flow_status_builder.buildFlowStatus({c2::FlowStatusRequest{"connection:Conn1:health"}});
  REQUIRE(status["connectionStatusList"].GetArray().Size() == 1);
  CHECK(status["connectionStatusList"].GetArray()[0]["id"] == "123fa7e6-2459-46dd-b2ba-61517239edf5");
  CHECK(status["connectionStatusList"].GetArray()[0]["name"] == "Conn1");
  CHECK(status["connectionStatusList"].GetArray()[0]["connectionHealth"]["queuedCount"].GetInt64() == 1);
  CHECK(status["connectionStatusList"].GetArray()[0]["connectionHealth"]["queuedBytes"].GetInt64() == 0);
  CHECK(status["errorsGeneratingReport"].Empty());
}

TEST_CASE("Build health status for all connections", "[flowstatusbuilder]") {
  c2::FlowStatusBuilder flow_status_builder;
  core::ProcessGroup process_group(core::ROOT_PROCESS_GROUP, "root");
  auto connection1 = std::make_unique<ConnectionImpl>(nullptr, nullptr, "Conn1", minifi::utils::Identifier::parse("123fa7e6-2459-46dd-b2ba-61517239edf5").value());
  auto connection2 = std::make_unique<ConnectionImpl>(nullptr, nullptr, "Conn2", minifi::utils::Identifier::parse("456fa7e6-2459-46dd-b2ba-61517239edf5").value());
  std::vector<std::shared_ptr<core::FlowFile>> flow_files1{std::make_shared<core::FlowFileImpl>()};
  connection1->multiPut(flow_files1);
  std::vector<std::shared_ptr<core::FlowFile>> flow_files2{std::make_shared<core::FlowFileImpl>(), std::make_shared<core::FlowFileImpl>()};
  connection2->multiPut(flow_files2);
  process_group.addConnection(std::move(connection1));
  process_group.addConnection(std::move(connection2));
  flow_status_builder.setRoot(&process_group);
  auto status = flow_status_builder.buildFlowStatus({c2::FlowStatusRequest{"connection:all:health"}});
  REQUIRE(status["connectionStatusList"].GetArray().Size() == 2);
  for (const auto& connection_status : status["connectionStatusList"].GetArray()) {
    std::string id = connection_status["id"].GetString();
    std::string name = connection_status["name"].GetString();
    bool id_and_name_check = (id == "123fa7e6-2459-46dd-b2ba-61517239edf5" && name == "Conn1") || (id == "456fa7e6-2459-46dd-b2ba-61517239edf5" && name == "Conn2");
    CHECK(id_and_name_check);
    if (id == "123fa7e6-2459-46dd-b2ba-61517239edf5") {
      CHECK(connection_status["connectionHealth"]["queuedCount"].GetInt64() == 1);
      CHECK(connection_status["connectionHealth"]["queuedBytes"].GetInt64() == 0);
    } else {
      CHECK(connection_status["connectionHealth"]["queuedCount"].GetInt64() == 2);
      CHECK(connection_status["connectionHealth"]["queuedBytes"].GetInt64() == 0);
    }
  }
  CHECK(status["errorsGeneratingReport"].Empty());
}

TEST_CASE("Non-existent connection generates an error", "[flowstatusbuilder]") {
  c2::FlowStatusBuilder flow_status_builder;
  core::ProcessGroup process_group(core::ROOT_PROCESS_GROUP, "root");
  auto connection = std::make_unique<ConnectionImpl>(nullptr, nullptr, "Conn1", minifi::utils::Identifier::parse("123fa7e6-2459-46dd-b2ba-61517239edf5").value());
  std::vector<std::shared_ptr<core::FlowFile>> flow_files{std::make_shared<core::FlowFileImpl>()};
  connection->multiPut(flow_files);
  process_group.addConnection(std::move(connection));
  flow_status_builder.setRoot(&process_group);
  auto status = flow_status_builder.buildFlowStatus({c2::FlowStatusRequest{"connection:InvalidConnection:health"}});
  CHECK(status["controllerServiceStatusList"].IsNull());
  CHECK(status["connectionStatusList"].Empty());
  CHECK(status["remoteProcessGroupStatusList"].IsNull());
  CHECK(status["instanceStatus"].IsNull());
  CHECK(status["systemDiagnosticsStatus"].IsNull());
  CHECK(status["processorStatusList"].IsNull());
  CHECK(status["errorsGeneratingReport"].GetArray().Size() == 1);
  CHECK(status["errorsGeneratingReport"].GetArray()[0].GetString() == std::string{"Unable to get connectionStatus: No connection with key 'InvalidConnection' to report status on"});
}

TEST_CASE("Build connection status with only invalid options", "[flowstatusbuilder]") {
  c2::FlowStatusBuilder flow_status_builder;
  core::ProcessGroup process_group(core::ROOT_PROCESS_GROUP, "root");
  auto connection = std::make_unique<ConnectionImpl>(nullptr, nullptr, "Conn1", minifi::utils::Identifier::parse("123fa7e6-2459-46dd-b2ba-61517239edf5").value());
  std::vector<std::shared_ptr<core::FlowFile>> flow_files{std::make_shared<core::FlowFileImpl>()};
  connection->multiPut(flow_files);
  process_group.addConnection(std::move(connection));
  flow_status_builder.setRoot(&process_group);
  auto status = flow_status_builder.buildFlowStatus({c2::FlowStatusRequest{"connection:Conn1:invalid1,invalid2"}});
  REQUIRE(status["connectionStatusList"].GetArray().Size() == 1);
  CHECK(status["connectionStatusList"].GetArray()[0]["id"] == "123fa7e6-2459-46dd-b2ba-61517239edf5");
  CHECK(status["connectionStatusList"].GetArray()[0]["name"] == "Conn1");
  CHECK(status["connectionStatusList"].GetArray()[0]["connectionHealth"].IsNull());
  CHECK(status["errorsGeneratingReport"].Empty());
}

TEST_CASE("Building connection status fails with incomplete query", "[flowstatusbuilder]") {
  c2::FlowStatusBuilder flow_status_builder;
  core::ProcessGroup process_group(core::ROOT_PROCESS_GROUP, "root");
  auto connection = std::make_unique<ConnectionImpl>(nullptr, nullptr, "Conn1", minifi::utils::Identifier::parse("123fa7e6-2459-46dd-b2ba-61517239edf5").value());
  std::vector<std::shared_ptr<core::FlowFile>> flow_files{std::make_shared<core::FlowFileImpl>()};
  connection->multiPut(flow_files);
  process_group.addConnection(std::move(connection));
  flow_status_builder.setRoot(&process_group);
  auto status = flow_status_builder.buildFlowStatus({c2::FlowStatusRequest{"connection:Conn1"}});
  CHECK(status["controllerServiceStatusList"].IsNull());
  CHECK(status["connectionStatusList"].Empty());
  CHECK(status["remoteProcessGroupStatusList"].IsNull());
  CHECK(status["instanceStatus"].IsNull());
  CHECK(status["systemDiagnosticsStatus"].IsNull());
  CHECK(status["processorStatusList"].IsNull());
  CHECK(status["errorsGeneratingReport"].GetArray().Size() == 1);
  CHECK(status["errorsGeneratingReport"].GetArray()[0].GetString() == std::string{"Unable to get connectionStatus: Query is incomplete"});
}

}  // namespace org::apache::nifi::minifi::test
