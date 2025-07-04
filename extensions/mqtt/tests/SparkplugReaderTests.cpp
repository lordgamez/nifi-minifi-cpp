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

#include "unit/Catch.h"
#include "unit/TestBase.h"
#include "../controllers/SparkplugReader.h"
#include "io/BufferStream.h"
#include "sparkplug_b.pb.h"

namespace org::apache::nifi::minifi::test {

TEST_CASE("Test", "[sparkplugReader]") {
  com::cirruslink::sparkplug::protobuf::Payload payload;
  payload.set_uuid("test-uuid");
  payload.set_timestamp(987654321);
  payload.set_seq(12345);
  payload.set_body("test-body");
  io::BufferStream buffer_stream;
  std::string payload_string;
  payload.SerializeToString(&payload_string);
  buffer_stream.write(reinterpret_cast<const uint8_t*>(payload_string.data()), payload_string.size());

  controllers::SparkplugReader sparkplug_reader("SparkplugReader");
  auto record_set = sparkplug_reader.read(buffer_stream);
  REQUIRE(record_set.has_value());
  REQUIRE(record_set->size() == 1);
  auto& record = record_set->at(0);
  REQUIRE(std::get<std::string>(record.at("uuid").value_) == "test-uuid");
}

}  // namespace org::apache::nifi::minifi::test
