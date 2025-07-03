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

#include "google/protobuf/message_lite.h"

#include "sparkplug_b.pb.h"

namespace org::apache::nifi::minifi::controllers {

namespace {
void sparkplugPayloadToRecord(const google::protobuf::Message& payload, core::Record& /*record*/) {
  const google::protobuf::Descriptor* descriptor = payload.GetDescriptor();
  const google::protobuf::Reflection* reflection = payload.GetReflection();

  // json_value.SetObject();

  int field_count = descriptor->field_count();
  for (int i = 0; i < field_count; ++i) {
      const google::protobuf::FieldDescriptor* field = descriptor->field(i);
      // const auto& field_name = field->name();

      if (field->is_repeated()) {
          int field_size = reflection->FieldSize(payload, field);
          // rapidjson::Value arr(rapidjson::kArrayType);
          for (int j = 0; j < field_size; ++j) {
              // rapidjson::Value value;
              switch (field->cpp_type()) {
                  case google::protobuf::FieldDescriptor::CPPTYPE_INT32:
                      // value.SetInt(reflection->GetRepeatedInt32(payload, field, j));
                      break;
                  case google::protobuf::FieldDescriptor::CPPTYPE_STRING:
                      // value.SetString(reflection->GetRepeatedString(payload, field, j), allocator);
                      break;
                  // handle other types...
                  default:
                      break;
              }
              // arr.PushBack(value, allocator);
          }
          // json_value.AddMember(rapidjson::Value(field_name, allocator).Move(), arr, allocator);
      } else {
          if (!reflection->HasField(payload, field)) {
              continue; // skip unset optional fields
          }
          rapidjson::Value value;
          switch (field->cpp_type()) {
              case google::protobuf::FieldDescriptor::CPPTYPE_INT32:
                  // value.SetInt(reflection->GetInt32(payload, field));
                  break;
              case google::protobuf::FieldDescriptor::CPPTYPE_STRING:
                  // value.SetString(reflection->GetString(payload, field), allocator);
                  break;
              case google::protobuf::FieldDescriptor::CPPTYPE_MESSAGE: {
                  const google::protobuf::Message& sub_message = reflection->GetMessage(payload, field);
                  (void)sub_message;
                  // rapidjson::Value sub_json;
                  // sparkplugPayloadToRecord(sub_message, sub_json, allocator);
                  // value = sub_json;
                  break;
              }
              // handle other types...
              default:
                  break;
          }
          // json_value.AddMember(rapidjson::Value(field_name, allocator).Move(), value, allocator);
      }
  }
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
  core::Record record;
  sparkplugPayloadToRecord(payload, record);
  core::RecordSet record_set;
  record_set.push_back(std::move(record));
  return record_set;
}

}  // namespace org::apache::nifi::minifi::controllers
