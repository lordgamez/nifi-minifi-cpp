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

#include "LmdbStream.h"
#include <algorithm>
#include <fstream>
#include <utility>
#include <vector>
#include <memory>
#include <string>
#include "minifi-cpp/Exception.h"
#include "io/validation.h"

namespace org::apache::nifi::minifi::io {

LmdbStream::LmdbStream(std::string path, MDB_env* lmdb_env, MDB_dbi* lmdb_handle, bool write_enable)
    : BaseStreamImpl(),
      path_(std::move(path)),
      write_enable_(write_enable),
      lmdb_env_(lmdb_env),
      lmdb_handle_(lmdb_handle),
      exists_([this] {
        MDB_val key{ path_.size(), const_cast<char*>(path_.data()) };
        MDB_val value{};

        MDB_txn* txn;
        mdb_txn_begin(lmdb_env_, nullptr, MDB_RDONLY, &txn);

        auto rc = mdb_get(txn, *lmdb_handle_, &key, &value);

        bool exists = false;
        if (rc == MDB_SUCCESS) {
          exists = true;
          value_ = std::string(static_cast<char*>(value.mv_data), value.mv_size);
        } else if (rc != MDB_NOTFOUND) {
          logger_->log_error("Failed to get value from LMDB database: {}", mdb_strerror(rc));
        }

        mdb_txn_abort(txn);
        return exists;
      }()),
      offset_(0),
      size_(value_.size()) {
}

void LmdbStream::close() {
}

void LmdbStream::seek(size_t offset) {
  offset_ = offset;
}

size_t LmdbStream::tell() const {
  return offset_;
}

size_t LmdbStream::write(const uint8_t *value, size_t size) {
  if (!write_enable_) return STREAM_ERROR;
  if (size != 0 && IsNullOrEmpty(value)) return STREAM_ERROR;


  // read the original value first to check existence
  MDB_val key{ path_.size(), const_cast<char*>(path_.data()) };
  MDB_val original_value{};

  MDB_txn* read_txn;
  mdb_txn_begin(lmdb_env_, nullptr, MDB_RDONLY, &read_txn);

  auto rc = mdb_get(read_txn, *lmdb_handle_, &key, &original_value);

  bool exists = false;
  if (rc == MDB_SUCCESS) {
    exists = true;
  } else if (rc != MDB_NOTFOUND) {
    logger_->log_error("Failed to get value from LMDB database: {}", mdb_strerror(rc));
    return STREAM_ERROR;
  }

  mdb_txn_abort(read_txn);

  // write value
  if (exists) {
    size += original_value.mv_size;
    // create MDB_val for merged value
    std::vector<uint8_t> merged_value(size);
    std::memcpy(merged_value.data(), original_value.mv_data, original_value.mv_size);
    std::memcpy(merged_value.data() + original_value.mv_size, value, size - original_value.mv_size);
    value = merged_value.data();
  }

  MDB_txn* write_txn;
  rc = mdb_txn_begin(lmdb_env_, nullptr, 0, &write_txn);
  if (rc != MDB_SUCCESS) {
    logger_->log_error("Failed to begin LMDB transaction: {}", mdb_strerror(rc));
    return STREAM_ERROR;
  }

  MDB_val val = { size, const_cast<uint8_t*>(value) };
  rc = mdb_put(write_txn, *lmdb_handle_, &key, &val, 0);
  if (rc != MDB_SUCCESS) {
    logger_->log_error("Failed to put value in LMDB database: {}", mdb_strerror(rc));
    mdb_txn_abort(write_txn);
    return STREAM_ERROR;
  }

  rc = mdb_txn_commit(write_txn);
  if (rc != MDB_SUCCESS) {
    logger_->log_error("Failed to commit LMDB transaction: {}", mdb_strerror(rc));
    mdb_txn_abort(write_txn);
    return STREAM_ERROR;
  }
  return size;
}

size_t LmdbStream::read(std::span<std::byte> buf) {
  if (!exists_) return STREAM_ERROR;
  if (buf.empty()) return 0;
  if (offset_ >= value_.size()) return 0;

  const auto amtToRead = std::min(buf.size(), value_.size() - offset_);
  std::memcpy(buf.data(), value_.data() + offset_, amtToRead);
  offset_ += amtToRead;
  return amtToRead;
}

}  // namespace org::apache::nifi::minifi::io
