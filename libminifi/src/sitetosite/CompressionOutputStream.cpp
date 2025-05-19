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
#include "sitetosite/CompressionOutputStream.h"

#include "io/ZlibStream.h"
#include "io/StreamPipe.h"
#include "io/BufferStream.h"
#include "core/logging/LoggerFactory.h"

namespace org::apache::nifi::minifi::sitetosite {

size_t CompressionOutputStream::write(const uint8_t *value, size_t len) {
  size_t free_spaces_left_in_buffer = buffer_.size() - buffer_index_;
  size_t bytes_left_to_write = len;
  size_t bytes_written = 0;
  while (bytes_left_to_write > 0) {
    size_t bytes_to_write = std::min(bytes_left_to_write, free_spaces_left_in_buffer);
    std::memcpy(reinterpret_cast<uint8_t*>(buffer_.data()) + buffer_index_, value + bytes_written, bytes_to_write);
    bytes_written += bytes_to_write;
    bytes_left_to_write -= bytes_to_write;
    buffer_index_ += bytes_to_write;
    if (buffer_index_ == buffer_.size()) {
      auto ret = compressAndWrite();
      if (io::isError(ret)) {
        return ret;
      }
    }
    free_spaces_left_in_buffer = buffer_.size() - buffer_index_;
  }
  return bytes_written;
}

size_t CompressionOutputStream::compressAndWrite() {
  if (was_data_written_) {
    auto ret = internal_stream_->write(static_cast<uint8_t>(1));
    if (io::isError(ret)) {
      return ret;
    }
  }
  auto ret = internal_stream_->write(reinterpret_cast<const uint8_t *>(SYNC_BYTES.data()), SYNC_BYTES.size());
  if (io::isError(ret)) {
    return ret;
  }

  was_data_written_ = true;

  io::BufferStream buffer_stream;
  {
    io::ZlibCompressStream zlib_stream{gsl::make_not_null(&buffer_stream), io::ZlibCompressionFormat::ZLIB, Z_BEST_SPEED};
    ret = zlib_stream.write(gsl::make_span(buffer_).subspan(0, buffer_index_));
    if (io::isError(ret)) {
      return ret;
    }
    gsl_Assert(buffer_index_ == ret);
    zlib_stream.close();
    gsl_Assert(zlib_stream.isFinished());
  }

  ret = internal_stream_->write(gsl::narrow<uint32_t>(buffer_index_));
  if (io::isError(ret)) {
    return ret;
  }

  size_t buffer_stream_size = buffer_stream.size();
  ret = internal_stream_->write(gsl::narrow<uint32_t>(buffer_stream_size));
  if (io::isError(ret)) {
    return ret;
  }

  ret = internal::pipe(buffer_stream, *internal_stream_);
  if (io::isError(ret)) {
    return ret;
  }

  buffer_index_ = 0;
  return ret;
}

void CompressionOutputStream::flush() {
  if (buffer_index_ > 0) {
    // TODO(lordgamez): Check for errors
    compressAndWrite();
  }
  if (was_data_written_) {
    internal_stream_->write(static_cast<uint8_t>(0));
    was_data_written_ = false;
  }
}

void CompressionOutputStream::close() {
  flush();
  internal_stream_->close();
}

}  // namespace org::apache::nifi::minifi::sitetosite
