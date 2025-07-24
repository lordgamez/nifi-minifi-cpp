/**
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
 * limitations under the License.c
 */
#include "catch2/generators/catch_generators.hpp"
#include "catch2/catch_approx.hpp"
#include "controllers/XMLReader.h"
#include "unit/Catch.h"
#include "unit/TestBase.h"

namespace org::apache::nifi::minifi::standard::test {

TEST_CASE("Invalid XML input or empty input results in error", "[XMLReader]") {
  const std::string xml_input = GENERATE("", "<invalid_xml>");
  io::BufferStream buffer_stream;
  buffer_stream.write(reinterpret_cast<const uint8_t*>(xml_input.data()), xml_input.size());

  XMLReader xml_reader("XMLReader");
  auto record_set = xml_reader.read(buffer_stream);
  REQUIRE_FALSE(record_set);
}

TEST_CASE("XML with only root node results in empty record set", "[XMLReader]") {
  const std::string xml_input = "<root></root>";
  io::BufferStream buffer_stream;
  buffer_stream.write(reinterpret_cast<const uint8_t*>(xml_input.data()), xml_input.size());

  XMLReader xml_reader("XMLReader");
  auto record_set = xml_reader.read(buffer_stream);
  REQUIRE(record_set);
  REQUIRE(record_set->empty());
}

TEST_CASE("XML with a single string child node results in a single record", "[XMLReader]") {
  const std::string xml_input = "<root><child>text</child></root>";
  io::BufferStream buffer_stream;
  buffer_stream.write(reinterpret_cast<const uint8_t*>(xml_input.data()), xml_input.size());

  XMLReader xml_reader("XMLReader");
  auto record_set = xml_reader.read(buffer_stream);
  REQUIRE(record_set);
  REQUIRE(record_set->size() == 1);
  auto& record = record_set->at(0);
  CHECK(std::get<std::string>(record.at("child").value_) == "text");
}

TEST_CASE("XML with several child nodes with different types result in a single record", "[XMLReader]") {
  const std::string xml_input = "<root><string>text</string><number>42</number><signed>-23</signed><boolean>true</boolean><double>3.14</double><timestamp>2023-03-15T12:34:56Z</timestamp></root>";
  io::BufferStream buffer_stream;
  buffer_stream.write(reinterpret_cast<const uint8_t*>(xml_input.data()), xml_input.size());

  XMLReader xml_reader("XMLReader");
  auto record_set = xml_reader.read(buffer_stream);
  REQUIRE(record_set);
  REQUIRE(record_set->size() == 1);
  auto& record = record_set->at(0);
  CHECK(std::get<std::string>(record.at("string").value_) == "text");
  CHECK(std::get<uint64_t>(record.at("number").value_) == 42);
  CHECK(std::get<int64_t>(record.at("signed").value_) == -23);
  CHECK(std::get<bool>(record.at("boolean").value_) == true);
  CHECK(std::get<double>(record.at("double").value_) == Catch::Approx(3.14));
  auto timestamp = std::get<std::chrono::system_clock::time_point>(record.at("timestamp").value_);
  auto expected_time = utils::timeutils::parseRfc3339("2023-03-15T12:34:56Z");
  REQUIRE(expected_time);
  CHECK(timestamp == *expected_time);
}

}  // namespace org::apache::nifi::minifi::standard::test
