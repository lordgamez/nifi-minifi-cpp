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
#include "../LmdbStream.h"
#include "lmdb.h"
#include "unit/Catch.h"
#include "unit/TestBase.h"

namespace org::apache::nifi::minifi::test {

class LmdbStreamTest : TestController {
 public:
  LmdbStreamTest() : db_path_(createTempDirectory().string()) {
    if (const int rc = mdb_env_create(&lmdb_env_)) {
      throw std::runtime_error("Failed to create LMDB environment: " + std::string(mdb_strerror(rc)));
    }
    mdb_env_set_maxdbs(lmdb_env_, 4);
    mdb_env_set_mapsize(lmdb_env_, static_cast<size_t>(1048576) * static_cast<size_t>(100000));

    if (const int rc = mdb_env_open(lmdb_env_, db_path_.c_str(), MDB_NOTLS, 0664)) {
      throw std::runtime_error("Failed to open LMDB environment " + db_path_ + ": " + std::string(mdb_strerror(rc)));
    }

    MDB_txn* init_txn = nullptr;
    mdb_txn_begin(lmdb_env_, nullptr, 0, &init_txn);
    if (const int rc = mdb_dbi_open(init_txn, nullptr, 0, &lmdb_handle_); rc != MDB_SUCCESS) {
      mdb_txn_abort(init_txn);
      mdb_env_close(lmdb_env_);
      throw std::runtime_error("Failed to open LMDB database: " + std::string(mdb_strerror(rc)));
    }
    mdb_txn_commit(init_txn);
  }

  ~LmdbStreamTest() override {
    mdb_dbi_close(lmdb_env_, lmdb_handle_);
    mdb_env_close(lmdb_env_);
  }

  std::optional<std::string> readValue(const std::string& key) {
    MDB_val db_value{};
    MDB_val db_key{key.size(), const_cast<char*>(key.data())};
    std::optional<std::string> return_value;

    MDB_txn* txn = nullptr;
    mdb_txn_begin(lmdb_env_, nullptr, MDB_RDONLY, &txn);

    auto result = mdb_get(txn, lmdb_handle_, &db_key, &db_value);
    if (result == MDB_SUCCESS) { return_value = std::string(static_cast<char*>(db_value.mv_data), db_value.mv_size); }

    mdb_txn_abort(txn);
    return return_value;
  }

 protected:
  std::string db_path_;
  MDB_env* lmdb_env_{nullptr};
  MDB_dbi lmdb_handle_{};
};

TEST_CASE_METHOD(LmdbStreamTest, "Simple write tests") {
  std::string content;
  SECTION("Write non-empty value") {
    content = "banana";
  }
  SECTION("Write empty value") {
    content = "";
  }

  {
    io::LmdbStream stream(db_path_, lmdb_env_, &lmdb_handle_, true);
    REQUIRE_FALSE(minifi::io::isError(stream.write(reinterpret_cast<const uint8_t*>(content.data()), content.size())));
  }
  auto val = readValue(db_path_);
  REQUIRE(val.has_value());
  REQUIRE(val->size() == content.size());
  REQUIRE(*val == content);
}

TEST_CASE_METHOD(LmdbStreamTest, "Multiple write test") {
  std::string content = "banana";
  std::string expected_content;

  {
    io::LmdbStream stream(db_path_, lmdb_env_, &lmdb_handle_, true);
    for (int i = 0; i < 5; ++i) {
      REQUIRE_FALSE(minifi::io::isError(stream.write(reinterpret_cast<const uint8_t*>(content.data()), content.size())));
      expected_content += content;
    }
  }
  auto val = readValue(db_path_);
  REQUIRE(val.has_value());
  REQUIRE(val->size() == expected_content.size());
  REQUIRE(*val == expected_content);
}

TEST_CASE_METHOD(LmdbStreamTest, "Simple read tests") {
  std::string content;
  SECTION("Write non-empty value") {
    content = "banana";
  }
  SECTION("Write empty value") {
    content = "";
  }

  io::LmdbStream write_stream(db_path_, lmdb_env_, &lmdb_handle_, true);
  REQUIRE_FALSE(minifi::io::isError(write_stream.write(reinterpret_cast<const uint8_t*>(content.data()), content.size())));
  write_stream.close();

  io::LmdbStream read_stream(db_path_, lmdb_env_, &lmdb_handle_, false);
  std::vector<std::byte> buffer(content.size());
  REQUIRE_FALSE(minifi::io::isError(read_stream.read(buffer)));
  std::string read_content(reinterpret_cast<char*>(buffer.data()), buffer.size());
  REQUIRE(read_content == content);
}

TEST_CASE_METHOD(LmdbStreamTest, "Read in chunks") {
  std::string content = "banana";
  io::LmdbStream write_stream(db_path_, lmdb_env_, &lmdb_handle_, true);
  REQUIRE_FALSE(minifi::io::isError(write_stream.write(reinterpret_cast<const uint8_t*>(content.data()), content.size())));
  write_stream.close();

  io::LmdbStream read_stream(db_path_, lmdb_env_, &lmdb_handle_, false);
  std::vector<std::byte> buffer(2);
  REQUIRE_FALSE(minifi::io::isError(read_stream.read(buffer)));
  std::string read_content(reinterpret_cast<char*>(buffer.data()), buffer.size());
  REQUIRE(read_content == "ba");

  REQUIRE_FALSE(minifi::io::isError(read_stream.read(buffer)));
  std::string read_content2(reinterpret_cast<char*>(buffer.data()), buffer.size());
  REQUIRE(read_content2 == "na");

  std::vector<std::byte> buffer2(5);
  auto read_result = read_stream.read(buffer2);
  REQUIRE_FALSE(minifi::io::isError(read_result));
  std::string read_content3(reinterpret_cast<char*>(buffer2.data()), read_result);
  REQUIRE(read_content3 == "na");
}

}  // namespace org::apache::nifi::minifi::test
