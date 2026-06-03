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

LmdbStream::LmdbStream(std::string path, bool write_enable)
    : BaseStreamImpl(),
      path_(std::move(path)),
      write_enable_(write_enable),
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
