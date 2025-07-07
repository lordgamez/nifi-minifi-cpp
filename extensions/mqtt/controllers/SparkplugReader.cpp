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
#include "google/protobuf/message.h"
#include "google/protobuf/descriptor.h"
#include "google/protobuf/reflection.h"

#include "sparkplug_b.pb.h"
#include "minifi-cpp/core/Record.h"
#include "utils/RecordUtils.h"
#include "core/Resource.h"

namespace org::apache::nifi::minifi::controllers {

namespace {

// nonstd::expected<core::Record, std::error_code> sparkplugPayloadToRecord(const com::cirruslink::sparkplug::protobuf::Payload& payload) {
//   std::string json_string;
//   auto status = google::protobuf::util::MessageToJsonString(payload, &json_string);
//   if (!status.ok()) {
//     return nonstd::make_unexpected(std::make_error_code(std::errc::invalid_argument));
//   }

//   rapidjson::Document document;
//   rapidjson::ParseResult parse_result = document.Parse(json_string.c_str());
//   if (!parse_result) {
//     return nonstd::make_unexpected(std::make_error_code(std::errc::invalid_argument));
//   }
//   return utils::record::parseRecord(document);
// }

core::RecordObject WalkAndSave(const google::protobuf::Message& message) {
  core::RecordObject result;
  const google::protobuf::Descriptor* descriptor = message.GetDescriptor();
  const google::protobuf::Reflection* reflection = message.GetReflection();

  int field_count = descriptor->field_count();
  for (int i = 0; i < field_count; ++i) {
    const google::protobuf::FieldDescriptor* field = descriptor->field(i);

    std::string full_name{field->name()};

    if (field->is_repeated()) {
      core::RecordArray record_array;
      int field_size = reflection->FieldSize(message, field);
      for (int j = 0; j < field_size; ++j) {
        switch (field->cpp_type()) {
          case google::protobuf::FieldDescriptor::CPPTYPE_INT32:
            record_array.push_back(core::RecordField(reflection->GetRepeatedInt32(message, field, j)));
            break;
          case google::protobuf::FieldDescriptor::CPPTYPE_INT64:
            record_array.push_back(core::RecordField(reflection->GetRepeatedInt64(message, field, j)));
            break;
          case google::protobuf::FieldDescriptor::CPPTYPE_STRING:
            record_array.push_back(core::RecordField(reflection->GetRepeatedString(message, field, j)));
            break;
          case google::protobuf::FieldDescriptor::CPPTYPE_MESSAGE: {
            record_array.push_back(core::RecordField{WalkAndSave(reflection->GetRepeatedMessage(message, field, j))});
            break;
          }
          // Add other types as needed
          default:
            break;
        }
      }
      result.emplace(full_name, core::BoxedRecordField{std::make_unique<core::RecordField>(record_array)});
    } else if (reflection->HasField(message, field)) {
      switch (field->cpp_type()) {
        case google::protobuf::FieldDescriptor::CPPTYPE_INT32:
          result.emplace(full_name, core::RecordField(reflection->GetInt32(message, field)));
          break;
        case google::protobuf::FieldDescriptor::CPPTYPE_INT64:
          result.emplace(full_name, core::RecordField(reflection->GetInt64(message, field)));
          break;
        case google::protobuf::FieldDescriptor::CPPTYPE_STRING:
          result.emplace(full_name, core::RecordField(reflection->GetString(message, field)));
          break;
        case google::protobuf::FieldDescriptor::CPPTYPE_BOOL:
          result.emplace(full_name, core::BoxedRecordField{std::make_unique<core::RecordField>(core::RecordField(reflection->GetBool(message, field)))};
          break;
        case google::protobuf::FieldDescriptor::CPPTYPE_DOUBLE:
          result.emplace(full_name, core::BoxedRecordField{std::make_unique<core::RecordField>(core::RecordField(reflection->GetDouble(message, field)))};
          break;
        case google::protobuf::FieldDescriptor::CPPTYPE_MESSAGE:
          result.emplace(full_name, core::RecordField(WalkAndSave(reflection->GetMessage(message, field))));
          break;
        // Add other types as needed
        default:
          break;
      }
    }
  }
  return result;
}

// CPPTYPE_INT32 = 1,     // TYPE_INT32, TYPE_SINT32, TYPE_SFIXED32
// CPPTYPE_INT64 = 2,     // TYPE_INT64, TYPE_SINT64, TYPE_SFIXED64
// CPPTYPE_UINT32 = 3,    // TYPE_UINT32, TYPE_FIXED32
// CPPTYPE_UINT64 = 4,    // TYPE_UINT64, TYPE_FIXED64
// CPPTYPE_DOUBLE = 5,    // TYPE_DOUBLE
// CPPTYPE_FLOAT = 6,     // TYPE_FLOAT
// CPPTYPE_BOOL = 7,      // TYPE_BOOL
// CPPTYPE_ENUM = 8,      // TYPE_ENUM
// CPPTYPE_STRING = 9,    // TYPE_STRING, TYPE_BYTES
// CPPTYPE_MESSAGE = 10,  // TYPE_MESSAGE, TYPE_GROUP

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
  auto record = WalkAndSave(payload);
  // if (!record) {
  //   return nonstd::make_unexpected(std::make_error_code(std::errc::invalid_argument));
  // }
  // record_set.push_back(std::move(record));
  return record_set;
}

REGISTER_RESOURCE(SparkplugReader, ControllerService);

}  // namespace org::apache::nifi::minifi::controllers
