/**
 * @file Site2SitePeer.cpp
 * Site2SitePeer class implementation
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
#include "sitetosite/CompressingSiteToSitePeer.h"

#include "io/ZlibStream.h"
#include "io/BufferStream.h"
#include "io/StreamPipe.h"

namespace org::apache::nifi::minifi::sitetosite {

size_t CompressingSiteToSitePeer::write(const uint8_t* data, size_t len) {
  auto ret = stream_->write(reinterpret_cast<const uint8_t *>(SYNC_BYTES.data()), SYNC_BYTES.size());
  if (io::isError(ret)) {
    return ret;
  }

  io::BufferStream buffer_stream;
  {
    io::ZlibCompressStream zlib_stream{gsl::make_not_null(&buffer_stream), io::ZlibCompressionFormat::ZLIB, 1};
    ret = zlib_stream.write(data, len);
    if (io::isError(ret)) {
      return ret;
    }
  }

  ret = stream_->write(buffer_stream.size());
  if (io::isError(ret)) {
    return ret;
  }

  ret = internal::pipe(buffer_stream, *stream_);
  if (io::isError(ret)) {
    return ret;
  }

  return stream_->write(1);
}

size_t CompressingSiteToSitePeer::read(std::span<std::byte> data) {
  auto ret = stream_->read(data);
  if (io::isError(ret)) {
    return ret;
  }

  // // check for SYNC bytes
  // if (std::equal(SYNC_BYTES.begin(), SYNC_BYTES.end(), data.begin())) {
  //   // read the length of the compressed data
  //   uint32_t compressed_length = 0;
  //   ret = stream_->read(compressed_length);
  //   if (io::isError(ret)) {
  //     return ret;
  //   }

  //   // read the compressed data
  //   io::BufferStream buffer_stream;
  //   ret = internal::pipe(*stream_, buffer_stream, compressed_length);
  //   if (io::isError(ret)) {
  //     return ret;
  //   }

  //   // decompress the data
  //   io::ZlibDecompressStream zlib_stream{&buffer_stream};
  //   ret = zlib_stream.read(data);
  //   if (io::isError(ret)) {
  //     return ret;
  //   }

  //   return ret;
  // }

  return -1;
}

void CompressingSiteToSitePeer::close() {
  if (stream_) {
    stream_->write(static_cast<uint64_t>(0));
  }
  SiteToSitePeer::close();
}

}  // namespace org::apache::nifi::minifi::sitetosite
