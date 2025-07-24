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

TEST_CASE("XML with multiple subnodes result in a single record with record object", "[XMLReader]") {
  const std::string xml_input = "<root><node><subnode1>text1</subnode1><subnode2><subsub1>text2</subsub1><subsub2>text3</subsub2></subnode2></node></root>";
  io::BufferStream buffer_stream;
  buffer_stream.write(reinterpret_cast<const uint8_t*>(xml_input.data()), xml_input.size());

  XMLReader xml_reader("XMLReader");
  auto record_set = xml_reader.read(buffer_stream);
  REQUIRE(record_set);
  REQUIRE(record_set->size() == 1);
  auto& record = record_set->at(0);
  auto record_object = std::get<core::RecordObject>(record.at("node").value_);
  REQUIRE(record_object.size() == 2);
  CHECK(std::get<std::string>(record_object.at("subnode1").value_) == "text1");
  CHECK(std::get<std::string>(std::get<core::RecordObject>(record_object.at("subnode2").value_).at("subsub1").value_) == "text2");
  CHECK(std::get<std::string>(std::get<core::RecordObject>(record_object.at("subnode2").value_).at("subsub2").value_) == "text3");
}

TEST_CASE("XML with nodes and text data is parsed correctly", "[XMLReader]") {
  const std::string xml_input = "<root>outtext1<node>nodetext<subnode>subtext</subnode></node>outtext2</root>";
  io::BufferStream buffer_stream;
  buffer_stream.write(reinterpret_cast<const uint8_t*>(xml_input.data()), xml_input.size());

  XMLReader xml_reader("XMLReader");
  auto record_set = xml_reader.read(buffer_stream);
  REQUIRE(record_set);
  REQUIRE(record_set->size() == 1);
  auto& record = record_set->at(0);
  CHECK(std::get<std::string>(std::get<core::RecordObject>(record.at("node").value_).at("subnode").value_) == "subtext");
  CHECK(std::get<std::string>(std::get<core::RecordObject>(record.at("node").value_).at("value").value_) == "nodetext");
  CHECK(std::get<std::string>(record.at("value").value_) == "outtext1outtext2");
}

}  // namespace org::apache::nifi::minifi::standard::test
