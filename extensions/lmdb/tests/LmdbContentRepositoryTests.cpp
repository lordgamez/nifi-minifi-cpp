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

#include "../LmdbContentRepository.h"
#include "ResourceClaim.h"
#include "properties/Configure.h"
#include "unit/Catch.h"
#include "unit/ContentRepositoryDependentTests.h"
#include "unit/TestBase.h"

namespace org::apache::nifi::minifi::test {

class LmdbContentRepositoryTests : TestController {
 public:
  LmdbContentRepositoryTests()
      : db_path_(createTempDirectory().string()),
        configuration_(std::make_shared<org::apache::nifi::minifi::ConfigureImpl>()),
        content_repo_(std::make_shared<core::repository::LmdbContentRepository>()) {
    configuration_->set(minifi::Configure::nifi_dbcontent_repository_directory_default, db_path_);
    REQUIRE(content_repo_->initialize(configuration_));
  }

 protected:
  std::string db_path_;
  std::shared_ptr<ConfigureImpl> configuration_;
  std::shared_ptr<core::repository::LmdbContentRepository> content_repo_;
};

TEST_CASE_METHOD(LmdbContentRepositoryTests, "Key does not exist in empty database", "[lmdb]") {
  auto claim = std::make_shared<minifi::ResourceClaimImpl>(content_repo_);
  REQUIRE_FALSE(content_repo_->exists(*claim));
}

TEST_CASE_METHOD(LmdbContentRepositoryTests, "Written value exists", "[lmdb]") {
  auto claim = std::make_shared<minifi::ResourceClaimImpl>(content_repo_);
  auto stream = content_repo_->write(*claim);
  const std::string content = "well hello there";
  stream->write(as_bytes(std::span(content)));
  stream->close();
  REQUIRE(content_repo_->exists(*claim));
}

TEST_CASE_METHOD(LmdbContentRepositoryTests, "Read written value", "[lmdb]") {
  auto claim = std::make_shared<minifi::ResourceClaimImpl>(content_repo_);
  auto write_stream = content_repo_->write(*claim);
  const std::string content = "well hello there";
  write_stream->write(as_bytes(std::span(content)));
  write_stream->close();
  auto read_stream = content_repo_->read(*claim);
  std::vector<std::byte> buffer(content.size());
  auto bytes_read = read_stream->read(as_writable_bytes(std::span(buffer)));
  read_stream->close();
  REQUIRE(bytes_read == content.size());
  std::string read_content(reinterpret_cast<char*>(buffer.data()), buffer.size());
  REQUIRE(read_content == content);
}

TEST_CASE_METHOD(LmdbContentRepositoryTests, "Delete value", "[lmdb]") {
  auto claim = std::make_shared<minifi::ResourceClaimImpl>(content_repo_);
  auto write_stream = content_repo_->write(*claim);
  const std::string content = "well hello there";
  write_stream->write(as_bytes(std::span(content)));
  write_stream->close();
  REQUIRE(content_repo_->exists(*claim));
  REQUIRE(content_repo_->remove(*claim));
  REQUIRE_FALSE(content_repo_->exists(*claim));
}

TEST_CASE_METHOD(LmdbContentRepositoryTests, "Clear orphan values", "[lmdb]") {
  auto claim = std::make_shared<minifi::ResourceClaimImpl>(content_repo_);
  auto write_stream = content_repo_->write(*claim);
  const std::string content = "well hello there";
  write_stream->write(as_bytes(std::span(content)));
  write_stream->close();
  REQUIRE(content_repo_->exists(*claim));
  content_repo_->reset();
  content_repo_->clearOrphans();
  REQUIRE_FALSE(content_repo_->exists(*claim));
}

TEST_CASE_METHOD(LmdbContentRepositoryTests, "Read database stats", "[lmdb]") {
  auto claim = std::make_shared<minifi::ResourceClaimImpl>(content_repo_);
  auto write_stream = content_repo_->write(*claim);
  const std::string content = "well hello there";
  write_stream->write(as_bytes(std::span(content)));
  write_stream->close();
  REQUIRE(content_repo_->getRepositoryEntryCount() == 1);
  REQUIRE(content_repo_->getRepositorySize() > 0);
}

TEST_CASE("ProcessSession::read reads the flowfile from offset to size", "[lmdb]") {
  ContentRepositoryDependentTests::testReadOnSmallerClonedFlowFiles(std::make_shared<core::repository::LmdbContentRepository>());
}

TEST_CASE("ProcessSession::append should append to the flowfile and set its size correctly", "[lmdb]") {
  ContentRepositoryDependentTests::testAppendToUnmanagedFlowFile(std::make_shared<core::repository::LmdbContentRepository>());
  ContentRepositoryDependentTests::testAppendToManagedFlowFile(std::make_shared<core::repository::LmdbContentRepository>());
}

TEST_CASE("ProcessSession::read can read zero length flowfiles without crash", "[lmdb]") {
  ContentRepositoryDependentTests::testReadFromZeroLengthFlowFile(std::make_shared<core::repository::LmdbContentRepository>());
}

TEST_CASE("ProcessSession::write can be cancelled", "[lmdb]") {
  ContentRepositoryDependentTests::testOkWrite(std::make_shared<core::repository::LmdbContentRepository>());
  ContentRepositoryDependentTests::testErrWrite(std::make_shared<core::repository::LmdbContentRepository>());
  ContentRepositoryDependentTests::testCancelWrite(std::make_shared<core::repository::LmdbContentRepository>());
}

}  // namespace org::apache::nifi::minifi::test
