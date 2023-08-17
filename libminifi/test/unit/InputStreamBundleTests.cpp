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
#include "../TestBase.h"
#include "../Catch.h"
#include "io/InputStreamBundle.h"
#include "io/BufferStream.h"

namespace org::apache::nifi::minifi::test {

TEST_CASE("Test reading from stream bundle with zero streams", "[testread]") {
  auto bundle = std::make_unique<minifi::io::InputStreamBundle>();

  std::unique_ptr<minifi::io::InputStream> input_stream = std::move(bundle);
  std::array<std::byte, 6> bytes;
  REQUIRE(0 == input_stream->read(bytes));
  REQUIRE(bytes == std::array<std::byte, 6>{std::byte(0x0), std::byte(0x0), std::byte(0x0), std::byte(0x0), std::byte(0x0), std::byte(0x0)});
}

TEST_CASE("Test reading from stream bundle with single stream", "[testread]") {
  auto bundled_stream = std::make_unique<minifi::io::BufferStream>();
  std::array<std::byte, 8> input1{std::byte(0x10), std::byte(0x20), std::byte(0x30), std::byte(0x40), std::byte(0x50), std::byte(0x60), std::byte(0x70), std::byte(0x80)};
  bundled_stream->write(input1);
  auto bundle = std::make_unique<minifi::io::InputStreamBundle>();
  bundle->addStream(std::move(bundled_stream));

  std::unique_ptr<minifi::io::InputStream> input_stream = std::move(bundle);
  std::array<std::byte, 6> bytes;
  REQUIRE(6 == input_stream->read(bytes));
  REQUIRE(bytes == std::array<std::byte, 6>{std::byte(0x10), std::byte(0x20), std::byte(0x30), std::byte(0x40), std::byte(0x50), std::byte(0x60)});
}

TEST_CASE("Test reading from stream bundle with two streams, but only part of the first stream fits in output buffer", "[testread]") {
  auto bundled_stream_1 = std::make_unique<minifi::io::BufferStream>();
  std::array<std::byte, 8> input1{std::byte(0x10), std::byte(0x20), std::byte(0x30), std::byte(0x40), std::byte(0x50), std::byte(0x60), std::byte(0x70), std::byte(0x80)};
  bundled_stream_1->write(input1);
  auto bundled_stream_2 = std::make_unique<minifi::io::BufferStream>();
  std::array<std::byte, 1> input2{std::byte(0x10)};
  bundled_stream_2->write(input2);
  auto bundle = std::make_unique<minifi::io::InputStreamBundle>();
  bundle->addStream(std::move(bundled_stream_1));
  bundle->addStream(std::move(bundled_stream_2));

  std::unique_ptr<minifi::io::InputStream> input_stream = std::move(bundle);
  std::array<std::byte, 6> bytes;
  REQUIRE(6 == input_stream->read(bytes));
  REQUIRE(bytes == std::array<std::byte, 6>{std::byte(0x10), std::byte(0x20), std::byte(0x30), std::byte(0x40), std::byte(0x50), std::byte(0x60)});
}

TEST_CASE("Test reading from stream bundle with two streams, but only the first stream and part of the second stream fits in output buffer", "[testread]") {
  auto bundled_stream_1 = std::make_unique<minifi::io::BufferStream>();
  std::array<std::byte, 4> input1{std::byte(0x10), std::byte(0x20), std::byte(0x30), std::byte(0x40)};
  bundled_stream_1->write(input1);
  auto bundled_stream_2 = std::make_unique<minifi::io::BufferStream>();
  std::array<std::byte, 4> input2{std::byte(0x50), std::byte(0x60), std::byte(0x70), std::byte(0x80)};
  bundled_stream_2->write(input2);
  auto bundle = std::make_unique<minifi::io::InputStreamBundle>();
  bundle->addStream(std::move(bundled_stream_1));
  bundle->addStream(std::move(bundled_stream_2));

  std::unique_ptr<minifi::io::InputStream> input_stream = std::move(bundle);
  std::array<std::byte, 6> bytes;
  REQUIRE(6 == input_stream->read(bytes));
  REQUIRE(bytes == std::array<std::byte, 6>{std::byte(0x10), std::byte(0x20), std::byte(0x30), std::byte(0x40), std::byte(0x50), std::byte(0x60)});
}

TEST_CASE("Test reading from stream bundle with two streams, both of them fits the output buffer", "[testread]") {
  auto bundled_stream_1 = std::make_unique<minifi::io::BufferStream>();
  std::array<std::byte, 4> input1{std::byte(0x10), std::byte(0x20), std::byte(0x30), std::byte(0x40)};
  bundled_stream_1->write(input1);
  auto bundled_stream_2 = std::make_unique<minifi::io::BufferStream>();
  std::array<std::byte, 4> input2{std::byte(0x50), std::byte(0x60), std::byte(0x70), std::byte(0x80)};
  bundled_stream_2->write(input2);
  auto bundle = std::make_unique<minifi::io::InputStreamBundle>();
  bundle->addStream(std::move(bundled_stream_1));
  bundle->addStream(std::move(bundled_stream_2));

  std::unique_ptr<minifi::io::InputStream> input_stream = std::move(bundle);
  std::array<std::byte, 9> bytes;
  REQUIRE(8 == input_stream->read(bytes));
  REQUIRE(bytes == std::array<std::byte, 9>{std::byte(0x10), std::byte(0x20), std::byte(0x30), std::byte(0x40), std::byte(0x50), std::byte(0x60), std::byte(0x70), std::byte(0x80), std::byte(0x0)});
}

TEST_CASE("Test reading from stream bundle with three streams, all fit the output buffer", "[testread]") {
  auto bundled_stream_1 = std::make_unique<minifi::io::BufferStream>();
  std::array<std::byte, 4> input1{std::byte(0x10), std::byte(0x20), std::byte(0x30), std::byte(0x40)};
  bundled_stream_1->write(input1);
  auto bundled_stream_2 = std::make_unique<minifi::io::BufferStream>();
  std::array<std::byte, 3> input2{std::byte(0x50), std::byte(0x60), std::byte(0x70)};
  bundled_stream_2->write(input2);
  auto bundled_stream_3 = std::make_unique<minifi::io::BufferStream>();
  std::array<std::byte, 2> input3{std::byte(0x80), std::byte(0x90)};
  bundled_stream_3->write(input3);
  auto bundle = std::make_unique<minifi::io::InputStreamBundle>();
  bundle->addStream(std::move(bundled_stream_1));
  bundle->addStream(std::move(bundled_stream_2));
  bundle->addStream(std::move(bundled_stream_3));

  std::unique_ptr<minifi::io::InputStream> input_stream = std::move(bundle);
  std::array<std::byte, 9> bytes;
  REQUIRE(9 == input_stream->read(bytes));
  REQUIRE(bytes == std::array<std::byte, 9>{std::byte(0x10), std::byte(0x20), std::byte(0x30), std::byte(0x40), std::byte(0x50), std::byte(0x60), std::byte(0x70), std::byte(0x80), std::byte(0x90)});
}

}  // namespace org::apache::nifi::minifi::test
