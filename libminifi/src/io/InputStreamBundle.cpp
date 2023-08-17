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
#include "io/InputStreamBundle.h"

namespace org::apache::nifi::minifi::io {

[[nodiscard]] size_t InputStreamBundle::size() const {
  size_t size = 0;
  for (const auto& stream : streams_) {
    size += stream->size();
  }
  return size;
}

size_t InputStreamBundle::read(std::span<std::byte> out_buffer) {
  if (streams_.empty()) {
    return 0;
  }
  size_t bytes_read = 0;
  while (bytes_read < out_buffer.size()) {
    if (stream_index_ >= streams_.size()) {
      break;
    }
    size_t next_read_size = std::min(streams_[stream_index_]->size() - stream_offset_, out_buffer.size() - bytes_read);
    auto current_read_bytes = streams_[stream_index_]->read(std::span(out_buffer).subspan(bytes_read, next_read_size));
    bytes_read += current_read_bytes;
    stream_offset_ += current_read_bytes;
    if (stream_offset_ >= streams_[stream_index_]->size()) {
      stream_offset_ = 0;
      ++stream_index_;
    }
  }
  return bytes_read;
}

void InputStreamBundle::addStream(std::unique_ptr<InputStream> stream) {
  if (!stream) {
    return;
  }

  streams_.push_back(gsl::make_not_null(std::move(stream)));
}

}  // namespace org::apache::nifi::minifi::io
