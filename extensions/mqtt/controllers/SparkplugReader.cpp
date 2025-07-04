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
 * limitations under the License.
 */
#include "SparkplugReader.h"

#include <vector>
#include <optional>

#include "google/protobuf/util/json_util.h"
#include "sparkplug_b.pb.h"
#include "minifi-cpp/core/Record.h"
#include "utils/RecordUtils.h"

namespace org::apache::nifi::minifi::controllers {

namespace {

nonstd::expected<core::Record, std::error_code> sparkplugPayloadToRecord(const com::cirruslink::sparkplug::protobuf::Payload& payload) {
  std::string json_string;
  auto status = google::protobuf::util::MessageToJsonString(payload, &json_string);
  if (!status.ok()) {
    return nonstd::make_unexpected(std::make_error_code(std::errc::invalid_argument));
  }

  rapidjson::Document document;
  rapidjson::ParseResult parse_result = document.Parse(json_string.c_str());
  if (!parse_result) {
    return nonstd::make_unexpected(std::make_error_code(std::errc::invalid_argument));
  }
  return utils::record::parseRecord(document);
}

}  // namespace

nonstd::expected<core::RecordSet, std::error_code> SparkplugReader::read(io::InputStream& input_stream) {
  com::cirruslink::sparkplug::protobuf::Payload payload;
  std::vector<std::byte> buffer(input_stream.size());
  auto result = input_stream.read(buffer);

  if (io::isError(result) || result != input_stream.size()) {
    return nonstd::make_unexpected(std::make_error_code(std::errc::invalid_argument));
  }

  if (!payload.ParseFromArray(static_cast<void*>(buffer.data()), input_stream.size())) {
    return nonstd::make_unexpected(std::make_error_code(std::errc::invalid_argument));
  }

  core::RecordSet record_set;
  auto record = sparkplugPayloadToRecord(payload);
  if (!record) {
    return nonstd::make_unexpected(std::make_error_code(std::errc::invalid_argument));
  }
  record_set.push_back(std::move(*record));
  return record_set;
}

}  // namespace org::apache::nifi::minifi::controllers
