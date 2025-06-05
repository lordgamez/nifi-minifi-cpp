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
#include "sitetosite/CompressionInputStream.h"
#include "io/ZlibStream.h"

namespace org::apache::nifi::minifi::sitetosite {

int64_t CompressionInputStream::decompressData() {
  if (eof_) {
    return 0;
  }

  std::vector<std::byte> local_buffer(DEFAULT_BUFFER_SIZE);
  auto ret = internal_stream_->read(std::span(local_buffer).subspan(0, SYNC_BYTES.size()));
  if (ret != SYNC_BYTES.size() ||
      !std::equal(SYNC_BYTES.begin(), SYNC_BYTES.end(), local_buffer.begin(), [](char sync_char, std::byte read_byte) { return static_cast<std::byte>(sync_char) == read_byte;})) {
    return io::STREAM_ERROR;
  }

  ret = internal_stream_->read(std::span(local_buffer).subspan(0, 4));
  if (io::isError(ret) || ret != 4) {
    return io::STREAM_ERROR;
  }

  uint32_t original_size = 0;
  std::memcpy(&original_size, local_buffer.data(), sizeof(uint32_t));

  ret = internal_stream_->read(std::span(local_buffer).subspan(0, 4));
  if (io::isError(ret) || ret != 4) {
    return io::STREAM_ERROR;
  }

  uint32_t compressed_size = 0;
  std::memcpy(&compressed_size, local_buffer.data(), sizeof(uint32_t));

  size_t bytes_read = internal_stream_->read(std::span(local_buffer).subspan(0, compressed_size));
  if (io::isError(bytes_read) || bytes_read != compressed_size) {
    return io::STREAM_ERROR;
  }

  io::BufferStream buffer_stream;
  {
    io::ZlibDecompressStream zlib_stream{gsl::make_not_null(&buffer_stream), io::ZlibCompressionFormat::ZLIB};
    ret = zlib_stream.write(gsl::make_span(buffer_).subspan(0, bytes_read));
    if (io::isError(ret)) {
      return ret;
    }
    zlib_stream.close();
    gsl_Assert(zlib_stream.isFinished());
  }

  ret = internal_stream_->read(std::span(local_buffer).subspan(0, 1));
  if (io::isError(ret) || ret != 1) {
    return io::STREAM_ERROR;
  }

  uint8_t end_byte = 0;
  std::memcpy(&end_byte, local_buffer.data(), sizeof(uint8_t));

  if (end_byte == 0) {
    eof_ = true;
  } else if (end_byte != 1) {
    return io::STREAM_ERROR;
  }

  buffered_data_length_ = bytes_read;
  buffer_index_ = 0;
  return bytes_read;
}

size_t CompressionInputStream::read(std::span<std::byte> out_buffer) {
  if (eof_ && buffered_data_length_ == 0) {
    return 0;
  }

  size_t bytes_to_read = out_buffer.size();
  while (bytes_to_read > 0) {
    if (buffered_data_length_ == 0) {
      decompressData();
    }
    uint64_t bytes_available = buffered_data_length_ - buffer_index_;
    if (bytes_available == 0) {
      break;
    }
    if (bytes_available <= bytes_to_read) {
      std::memcpy(out_buffer.data(), reinterpret_cast<const uint8_t*>(buffer_.data()) + buffer_index_, bytes_available);
      buffer_index_ = 0;
      buffered_data_length_ = 0;
      bytes_to_read -= bytes_available;
    } else {
      std::memcpy(out_buffer.data(), reinterpret_cast<const uint8_t*>(buffer_.data()) + buffer_index_, bytes_to_read);
      buffer_index_ += bytes_to_read;
      buffered_data_length_ -= bytes_to_read;
      bytes_to_read = 0;
    }
  }

  return 0;
}

void CompressionInputStream::close() {
  internal_stream_->close();
}

}  // namespace org::apache::nifi::minifi::sitetosite
