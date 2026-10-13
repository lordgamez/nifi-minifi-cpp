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
#include <string>

#include "unit/TestBase.h"
#include "unit/Catch.h"
#include "OpcUaTestServer.h"
#include "unit/SingleProcessorTestController.h"
#include "include/FetchOPCFile.h"
#include "unit/TestUtils.h"

namespace org::apache::nifi::minifi::test {

namespace {

constexpr const char* ENDPOINT = "opc.tcp://127.0.0.1:4844/";

class FetchOPCFileTestFixture {
 public:
  FetchOPCFileTestFixture() {
    files_node_ = server_.addFileDirectory("Files", UA_NODEID_STRING(server_.getNamespaceIndex(), const_cast<char*>("Files")),
        UA_NODEID_NUMERIC(0, UA_NS0ID_OBJECTSFOLDER));
    server_.start();
    REQUIRE(processor_->setProperty(processors::FetchOPCFile::OPCServerEndPoint.name, ENDPOINT));
    REQUIRE(processor_->setProperty(processors::FetchOPCFile::NameSpaceIndex.name, std::to_string(server_.getNamespaceIndex())));
  }

 protected:
  OpcUaTestServer server_{4844};
  opc::NodeId files_node_;
  SingleProcessorTestController controller_{minifi::test::utils::make_processor<processors::FetchOPCFile>("FetchOPCFile")};
  core::Processor* processor_ = controller_.getProcessor();
};

}  // namespace

TEST_CASE_METHOD(FetchOPCFileTestFixture, "Fetch file using string node id", "[fetchopcfile]") {
  server_.addFile("test.txt", UA_NODEID_STRING(server_.getNamespaceIndex(), const_cast<char*>("test.txt")), files_node_.get(), "file content");
  REQUIRE(processor_->setProperty(processors::FetchOPCFile::NodeIDType.name, "String"));
  REQUIRE(processor_->setProperty(processors::FetchOPCFile::NodeID.name, "test.txt"));

  const auto results = controller_.trigger();
  REQUIRE(results.at(processors::FetchOPCFile::Success).size() == 1);
  const auto& flow_file = results.at(processors::FetchOPCFile::Success)[0];
  CHECK(controller_.plan->getContent(flow_file) == "file content");
  CHECK(flow_file->getAttribute("Browsename") == "test.txt");
  CHECK(flow_file->getAttribute("NodeID") == "test.txt");
  CHECK(flow_file->getAttribute("NodeID type") == "string");
  CHECK(flow_file->getAttribute("Namespace index") == std::to_string(server_.getNamespaceIndex()));
  CHECK(server_.getOpenFileHandleCount("test.txt") == 0);
}

TEST_CASE_METHOD(FetchOPCFileTestFixture, "Fetch file using int node id", "[fetchopcfile]") {
  server_.addFile("int_file.txt", UA_NODEID_NUMERIC(server_.getNamespaceIndex(), 7777), files_node_.get(), "int content");
  REQUIRE(processor_->setProperty(processors::FetchOPCFile::NodeIDType.name, "Int"));
  REQUIRE(processor_->setProperty(processors::FetchOPCFile::NodeID.name, "7777"));

  const auto results = controller_.trigger();
  REQUIRE(results.at(processors::FetchOPCFile::Success).size() == 1);
  const auto& flow_file = results.at(processors::FetchOPCFile::Success)[0];
  CHECK(controller_.plan->getContent(flow_file) == "int content");
  CHECK(flow_file->getAttribute("NodeID") == "7777");
  CHECK(flow_file->getAttribute("NodeID type") == "numeric");
  CHECK(flow_file->getAttribute("Browsename") == "int_file.txt");
}

TEST_CASE_METHOD(FetchOPCFileTestFixture, "Fetch file using guid node id", "[fetchopcfile]") {
  server_.addFile("guid_file.txt", UA_NODEID_GUID(server_.getNamespaceIndex(), UA_GUID("11223344-5566-7788-99aa-bbccddeeff00")), files_node_.get(), "guid content");
  REQUIRE(processor_->setProperty(processors::FetchOPCFile::NodeIDType.name, "Guid"));
  REQUIRE(processor_->setProperty(processors::FetchOPCFile::NodeID.name, "11223344-5566-7788-99aa-bbccddeeff00"));

  const auto results = controller_.trigger();
  REQUIRE(results.at(processors::FetchOPCFile::Success).size() == 1);
  const auto& flow_file = results.at(processors::FetchOPCFile::Success)[0];
  CHECK(controller_.plan->getContent(flow_file) == "guid content");
  CHECK(flow_file->getAttribute("NodeID") == "11223344-5566-7788-99aa-bbccddeeff00");
  CHECK(flow_file->getAttribute("NodeID type") == "guid");
}

TEST_CASE_METHOD(FetchOPCFileTestFixture, "Fetch file using path node id", "[fetchopcfile]") {
  server_.addFile("path_file.txt", UA_NODEID_NUMERIC(server_.getNamespaceIndex(), 0), files_node_.get(), "path content");
  REQUIRE(processor_->setProperty(processors::FetchOPCFile::NodeIDType.name, "Path"));
  REQUIRE(processor_->setProperty(processors::FetchOPCFile::NodeID.name, "Files/path_file.txt"));
  REQUIRE(processor_->setProperty(processors::FetchOPCFile::PathReferenceTypes.name, "HasComponent"));

  const auto results = controller_.trigger();
  REQUIRE(results.at(processors::FetchOPCFile::Success).size() == 1);
  const auto& flow_file = results.at(processors::FetchOPCFile::Success)[0];
  CHECK(controller_.plan->getContent(flow_file) == "path content");
  CHECK(flow_file->getAttribute("Browsename") == "path_file.txt");
  // The attributes describe the node the path is resolved to, which got a server assigned numeric node id
  CHECK(flow_file->getAttribute("NodeID type") == "numeric");
  CHECK(flow_file->getAttribute("Namespace index") == std::to_string(server_.getNamespaceIndex()));
}

TEST_CASE_METHOD(FetchOPCFileTestFixture, "Fetch file larger than the chunk size", "[fetchopcfile]") {
  std::string content;
  for (size_t i = 0; i < 1000; ++i) {
    content += std::to_string(i) + ",";
  }
  server_.addFile("large.txt", UA_NODEID_STRING(server_.getNamespaceIndex(), const_cast<char*>("large.txt")), files_node_.get(), content);
  REQUIRE(processor_->setProperty(processors::FetchOPCFile::NodeIDType.name, "String"));
  REQUIRE(processor_->setProperty(processors::FetchOPCFile::NodeID.name, "large.txt"));
  REQUIRE(processor_->setProperty(processors::FetchOPCFile::ChunkSize.name, "100 B"));

  const auto results = controller_.trigger();
  REQUIRE(results.at(processors::FetchOPCFile::Success).size() == 1);
  CHECK(controller_.plan->getContent(results.at(processors::FetchOPCFile::Success)[0]) == content);
  CHECK(server_.getMaxTransferredChunkSize("large.txt") == 100);
}

TEST_CASE_METHOD(FetchOPCFileTestFixture, "Fetch file with chunk size limited by the MaxByteStringLength property of the server", "[fetchopcfile]") {
  const std::string content(1000, 'a');
  server_.addFile("limited.txt", UA_NODEID_STRING(server_.getNamespaceIndex(), const_cast<char*>("limited.txt")), files_node_.get(), content, true, 64);
  REQUIRE(processor_->setProperty(processors::FetchOPCFile::NodeIDType.name, "String"));
  REQUIRE(processor_->setProperty(processors::FetchOPCFile::NodeID.name, "limited.txt"));

  const auto results = controller_.trigger();
  REQUIRE(results.at(processors::FetchOPCFile::Success).size() == 1);
  CHECK(controller_.plan->getContent(results.at(processors::FetchOPCFile::Success)[0]) == content);
  CHECK(server_.getMaxTransferredChunkSize("limited.txt") == 64);
}

TEST_CASE_METHOD(FetchOPCFileTestFixture, "Fetch empty file", "[fetchopcfile]") {
  server_.addFile("empty.txt", UA_NODEID_STRING(server_.getNamespaceIndex(), const_cast<char*>("empty.txt")), files_node_.get(), "");
  REQUIRE(processor_->setProperty(processors::FetchOPCFile::NodeIDType.name, "String"));
  REQUIRE(processor_->setProperty(processors::FetchOPCFile::NodeID.name, "empty.txt"));

  const auto results = controller_.trigger();
  REQUIRE(results.at(processors::FetchOPCFile::Success).size() == 1);
  CHECK(controller_.plan->getContent(results.at(processors::FetchOPCFile::Success)[0]).empty());
}

TEST_CASE_METHOD(FetchOPCFileTestFixture, "Fetching a non-existent file node yields without creating a flowfile", "[fetchopcfile]") {
  REQUIRE(processor_->setProperty(processors::FetchOPCFile::NodeIDType.name, "String"));
  REQUIRE(processor_->setProperty(processors::FetchOPCFile::NodeID.name, "missing.txt"));

  const auto results = controller_.trigger();
  CHECK(results.at(processors::FetchOPCFile::Success).empty());
  CHECK(processor_->isYield());
}

TEST_CASE_METHOD(FetchOPCFileTestFixture, "Fetching a node which is not a file yields without creating a flowfile", "[fetchopcfile]") {
  REQUIRE(processor_->setProperty(processors::FetchOPCFile::NodeIDType.name, "String"));
  REQUIRE(processor_->setProperty(processors::FetchOPCFile::NodeID.name, "Files"));

  const auto results = controller_.trigger();
  CHECK(results.at(processors::FetchOPCFile::Success).empty());
  CHECK(processor_->isYield());
}

TEST_CASE_METHOD(FetchOPCFileTestFixture, "Chunk size of zero is invalid", "[fetchopcfile]") {
  REQUIRE(processor_->setProperty(processors::FetchOPCFile::NodeIDType.name, "String"));
  REQUIRE(processor_->setProperty(processors::FetchOPCFile::NodeID.name, "test.txt"));
  REQUIRE(processor_->setProperty(processors::FetchOPCFile::ChunkSize.name, "0 B"));

  REQUIRE_THROWS_WITH(controller_.trigger(), "Process Schedule Operation: Chunk size must be between 1 and 2147483647 bytes");
}

TEST_CASE_METHOD(FetchOPCFileTestFixture, "Chunk size larger than the maximum byte string length is invalid", "[fetchopcfile]") {
  REQUIRE(processor_->setProperty(processors::FetchOPCFile::NodeIDType.name, "String"));
  REQUIRE(processor_->setProperty(processors::FetchOPCFile::NodeID.name, "test.txt"));
  REQUIRE(processor_->setProperty(processors::FetchOPCFile::ChunkSize.name, "3 GB"));

  REQUIRE_THROWS_WITH(controller_.trigger(), "Process Schedule Operation: Chunk size must be between 1 and 2147483647 bytes");
}

TEST_CASE_METHOD(FetchOPCFileTestFixture, "Chunk size is used if the MaxByteStringLength property does not limit it", "[fetchopcfile]") {
  const std::string content(1000, 'a');
  REQUIRE(processor_->setProperty(processors::FetchOPCFile::NodeIDType.name, "String"));
  REQUIRE(processor_->setProperty(processors::FetchOPCFile::NodeID.name, "file.txt"));
  REQUIRE(processor_->setProperty(processors::FetchOPCFile::ChunkSize.name, "100 B"));

  SECTION("MaxByteStringLength is larger than the chunk size") {
    server_.addFile("file.txt", UA_NODEID_STRING(server_.getNamespaceIndex(), const_cast<char*>("file.txt")), files_node_.get(), content, true, 500);
  }
  SECTION("MaxByteStringLength of zero means no limit") {
    server_.addFile("file.txt", UA_NODEID_STRING(server_.getNamespaceIndex(), const_cast<char*>("file.txt")), files_node_.get(), content, true, 0);
  }

  const auto results = controller_.trigger();
  REQUIRE(results.at(processors::FetchOPCFile::Success).size() == 1);
  CHECK(controller_.plan->getContent(results.at(processors::FetchOPCFile::Success)[0]) == content);
  CHECK(server_.getMaxTransferredChunkSize("file.txt") == 100);
}

TEST_CASE_METHOD(FetchOPCFileTestFixture, "Fetch binary file content", "[fetchopcfile]") {
  const std::string content("\x00\x01\xff\xfe\x00" "binary" "\x80\x00", 13);
  server_.addFile("binary.bin", UA_NODEID_STRING(server_.getNamespaceIndex(), const_cast<char*>("binary.bin")), files_node_.get(), content);
  REQUIRE(processor_->setProperty(processors::FetchOPCFile::NodeIDType.name, "String"));
  REQUIRE(processor_->setProperty(processors::FetchOPCFile::NodeID.name, "binary.bin"));

  const auto results = controller_.trigger();
  REQUIRE(results.at(processors::FetchOPCFile::Success).size() == 1);
  CHECK(controller_.plan->getContent(results.at(processors::FetchOPCFile::Success)[0]) == content);
}

TEST_CASE_METHOD(FetchOPCFileTestFixture, "File is fetched again on every trigger", "[fetchopcfile]") {
  server_.addFile("test.txt", UA_NODEID_STRING(server_.getNamespaceIndex(), const_cast<char*>("test.txt")), files_node_.get(), "file content");
  REQUIRE(processor_->setProperty(processors::FetchOPCFile::NodeIDType.name, "String"));
  REQUIRE(processor_->setProperty(processors::FetchOPCFile::NodeID.name, "test.txt"));

  for (size_t i = 0; i < 2; ++i) {
    const auto results = controller_.trigger();
    REQUIRE(results.at(processors::FetchOPCFile::Success).size() == 1);
    CHECK(controller_.plan->getContent(results.at(processors::FetchOPCFile::Success)[0]) == "file content");
  }
  CHECK(server_.getOpenFileHandleCount("test.txt") == 0);
}

TEST_CASE_METHOD(FetchOPCFileTestFixture, "Unreachable server yields without creating a flowfile", "[fetchopcfile]") {
  REQUIRE(processor_->setProperty(processors::FetchOPCFile::OPCServerEndPoint.name, "opc.tcp://127.0.0.1:4899/"));
  REQUIRE(processor_->setProperty(processors::FetchOPCFile::NodeIDType.name, "String"));
  REQUIRE(processor_->setProperty(processors::FetchOPCFile::NodeID.name, "test.txt"));

  const auto results = controller_.trigger();
  CHECK(results.at(processors::FetchOPCFile::Success).empty());
  CHECK(processor_->isYield());
}

TEST_CASE_METHOD(FetchOPCFileTestFixture, "Path which cannot be resolved to a single node yields without creating a flowfile", "[fetchopcfile]") {
  REQUIRE(processor_->setProperty(processors::FetchOPCFile::NodeIDType.name, "Path"));

  SECTION("Path does not exist") {
    REQUIRE(processor_->setProperty(processors::FetchOPCFile::NodeID.name, "Files/missing.txt"));
    REQUIRE(processor_->setProperty(processors::FetchOPCFile::PathReferenceTypes.name, "HasComponent"));
  }
  SECTION("Path resolves to multiple nodes") {
    REQUIRE(processor_->setProperty(processors::FetchOPCFile::NodeID.name, "Simulator/Default/AmbiguousParent/Ambiguous"));
  }

  const auto results = controller_.trigger();
  CHECK(results.at(processors::FetchOPCFile::Success).empty());
  CHECK(processor_->isYield());
}

TEST_CASE_METHOD(FetchOPCFileTestFixture, "Failing to open the file yields without creating a flowfile", "[fetchopcfile]") {
  server_.addFile("test.txt", UA_NODEID_STRING(server_.getNamespaceIndex(), const_cast<char*>("test.txt")), files_node_.get(), "file content");
  server_.setFileFailure("test.txt", FileMethod::Open, UA_STATUSCODE_BADNOTREADABLE);
  REQUIRE(processor_->setProperty(processors::FetchOPCFile::NodeIDType.name, "String"));
  REQUIRE(processor_->setProperty(processors::FetchOPCFile::NodeID.name, "test.txt"));

  const auto results = controller_.trigger();
  CHECK(results.at(processors::FetchOPCFile::Success).empty());
  CHECK(processor_->isYield());
}

TEST_CASE_METHOD(FetchOPCFileTestFixture, "Failing to read the file yields without creating a flowfile and closes the file", "[fetchopcfile]") {
  server_.addFile("test.txt", UA_NODEID_STRING(server_.getNamespaceIndex(), const_cast<char*>("test.txt")), files_node_.get(), std::string(1000, 'a'));
  server_.setFileFailure("test.txt", FileMethod::Read, UA_STATUSCODE_BADINTERNALERROR, 1);
  REQUIRE(processor_->setProperty(processors::FetchOPCFile::NodeIDType.name, "String"));
  REQUIRE(processor_->setProperty(processors::FetchOPCFile::NodeID.name, "test.txt"));
  REQUIRE(processor_->setProperty(processors::FetchOPCFile::ChunkSize.name, "100 B"));

  const auto results = controller_.trigger();
  CHECK(results.at(processors::FetchOPCFile::Success).empty());
  CHECK(processor_->isYield());
  CHECK(server_.getOpenFileHandleCount("test.txt") == 0);
}

TEST_CASE_METHOD(FetchOPCFileTestFixture, "Failing to close the file after reading it still transfers the flowfile", "[fetchopcfile]") {
  LogTestController::getInstance().setWarn<processors::FetchOPCFile>();
  server_.addFile("test.txt", UA_NODEID_STRING(server_.getNamespaceIndex(), const_cast<char*>("test.txt")), files_node_.get(), "file content");
  server_.setFileFailure("test.txt", FileMethod::Close, UA_STATUSCODE_BADINTERNALERROR);
  REQUIRE(processor_->setProperty(processors::FetchOPCFile::NodeIDType.name, "String"));
  REQUIRE(processor_->setProperty(processors::FetchOPCFile::NodeID.name, "test.txt"));

  const auto results = controller_.trigger();
  REQUIRE(results.at(processors::FetchOPCFile::Success).size() == 1);
  CHECK(controller_.plan->getContent(results.at(processors::FetchOPCFile::Success)[0]) == "file content");
  CHECK(LogTestController::getInstance().contains("Failed to close file node 'test.txt' after reading it: BadInternalError"));
}

}  // namespace org::apache::nifi::minifi::test
