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
#pragma once

#include <iostream>
#include <cstdint>
#include <string>
#include <memory>
#include "io/BaseStream.h"
#include "core/logging/LoggerFactory.h"
#include "lmdb.h"

namespace org::apache::nifi::minifi::io {

class LmdbStream : public io::BaseStreamImpl {
 public:
  explicit LmdbStream(std::string path, MDB_env* lmdb_env, MDB_dbi* lmdb_handle, bool write_enable = false);

  ~LmdbStream() override {
    close();
  }

  void close() final;
  /**
   * Skip to the specified offset.
   * @param offset offset to which we will skip
   */
  void seek(size_t offset) override;

  size_t tell() const override;

  size_t size() const override {
    return size_;
  }

  using BaseStream::write;
  using BaseStream::read;

  size_t read(std::span<std::byte> buf) override;
  size_t write(const uint8_t *value, size_t size) override;

 protected:
  std::string path_;
  bool write_enable_;
  std::string value_;
  MDB_env* lmdb_env_;
  MDB_dbi* lmdb_handle_;
  bool exists_;
  size_t offset_;
  size_t size_;
  bool use_synchronous_writes_;

 private:
  std::shared_ptr<core::logging::Logger> logger_ = core::logging::LoggerFactory<LmdbStream>::getLogger();
};

}  // namespace org::apache::nifi::minifi::io
