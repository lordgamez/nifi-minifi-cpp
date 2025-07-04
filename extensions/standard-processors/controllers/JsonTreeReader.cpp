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

#include "JsonTreeReader.h"

#include "core/Resource.h"
#include "utils/RecordUtils.h"

namespace org::apache::nifi::minifi::standard {

bool readAsJsonLines(const std::string& content, core::RecordSet& record_set) {
  std::stringstream ss(content);
  std::string line;
  while (std::getline(ss, line, '\n')) {
    rapidjson::Document document;
    if (rapidjson::ParseResult parse_result = document.Parse<rapidjson::kParseStopWhenDoneFlag>(line); parse_result.IsError())
      return false;
    auto record = utils::record::parseRecord(document);
    if (!record)
      return false;
    record_set.push_back(std::move(*record));
  }
  return true;
}

bool readAsArray(const std::string& content, core::RecordSet& record_set) {
  rapidjson::Document document;
  if (const rapidjson::ParseResult parse_result = document.Parse<rapidjson::kParseStopWhenDoneFlag>(content); parse_result.IsError())
    return false;
  if (!document.IsArray())
    return false;
  for (auto& json_record : document.GetArray()) {
    auto record  = utils::record::parseRecord(json_record);
    if (!record)
      return false;
    record_set.push_back(std::move(*record));
  }
  return true;
}

nonstd::expected<core::RecordSet, std::error_code> JsonTreeReader::read(io::InputStream& input_stream) {
  core::RecordSet record_set{};
  const auto read_result = [&record_set](io::InputStream& input_stream) -> int64_t {
    std::string content;
    content.resize(input_stream.size());
    const auto read_ret = gsl::narrow<int64_t>(input_stream.read(as_writable_bytes(std::span(content))));
    if (io::isError(read_ret)) {
      return -1;
    }
    if (content.starts_with('[')) {
      readAsArray(content, record_set);
    } else {
      readAsJsonLines(content, record_set);
    }
    return read_ret;
  }(input_stream);
  if (io::isError(read_result))
    return nonstd::make_unexpected(std::make_error_code(std::errc::invalid_argument));
  return record_set;
}

REGISTER_RESOURCE(JsonTreeReader, ControllerService);
}  // namespace org::apache::nifi::minifi::standard

#ifdef WIN32
#pragma pop_macro("GetObject")
#endif
