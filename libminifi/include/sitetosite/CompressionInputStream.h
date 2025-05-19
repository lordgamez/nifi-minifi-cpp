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

#include "io/InputStream.h"
#include "io/BufferStream.h"

namespace org::apache::nifi::minifi::sitetosite {

class CompressionInputStream : public io::InputStreamImpl {
 public:
  static constexpr size_t DEFAULT_BUFFER_SIZE = 65536;
  static constexpr std::array<char, 4> SYNC_BYTES = { 'S', 'Y', 'N', 'C' };

  explicit CompressionInputStream(gsl::not_null<io::BaseStream*> internal_stream)
      : internal_stream_(internal_stream) {
  }

  using io::InputStream::read;

  size_t read(std::span<std::byte> out_buffer) override;

  void close() override;

 private:
  gsl::not_null<io::BaseStream*> internal_stream_;
  std::vector<std::byte> buffer_{};
  size_t buffer_index_{0};
};

}  // namespace org::apache::nifi::minifi::sitetosite
