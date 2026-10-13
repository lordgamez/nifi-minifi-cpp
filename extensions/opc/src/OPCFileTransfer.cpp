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
#include "OPCFileTransfer.h"

#include <array>
#include <string>
#include <utility>

namespace org::apache::nifi::minifi::opc {

std::expected<RemoteFile, UA_StatusCode> RemoteFile::create(Client& client, NodeId file_node_id) {
  RemoteFile file(client, std::move(file_node_id));
  const std::array<std::pair<std::string_view, NodeId*>, 4> methods{{
      {"Open", &file.open_method_id_},
      {"Close", &file.close_method_id_},
      {"Read", &file.read_method_id_},
      {"Write", &file.write_method_id_}}};
  for (const auto& [name, method_id] : methods) {
    auto result = client.findChild(file.file_node_id_, name, 0, UA_NS0ID_HASCOMPONENT);
    if (!result) {
      return std::unexpected{result.error()};
    }
    *method_id = std::move(*result);
  }
  return file;
}

std::expected<UA_UInt32, UA_StatusCode> RemoteFile::open(UA_Byte mode) {
  UA_Variant input;
  UA_Variant_setScalar(&input, &mode, &UA_TYPES[UA_TYPES_BYTE]);
  auto output = client_->callMethod(file_node_id_, open_method_id_, std::span(&input, 1));
  if (!output) {
    return std::unexpected{output.error()};
  }
  const auto values = output->values();
  if (values.size() != 1 || !UA_Variant_hasScalarType(&values[0], &UA_TYPES[UA_TYPES_UINT32])) {
    return std::unexpected{UA_STATUSCODE_BADTYPEMISMATCH};
  }
  return *static_cast<const UA_UInt32*>(values[0].data);
}

UA_StatusCode RemoteFile::close(UA_UInt32 handle) {
  UA_Variant input;
  UA_Variant_setScalar(&input, &handle, &UA_TYPES[UA_TYPES_UINT32]);
  auto output = client_->callMethod(file_node_id_, close_method_id_, std::span(&input, 1));
  return output ? UA_STATUSCODE_GOOD : output.error();
}

std::expected<std::vector<std::byte>, UA_StatusCode> RemoteFile::read(UA_UInt32 handle, UA_Int32 length) {
  std::array<UA_Variant, 2> inputs{};
  UA_Variant_setScalar(&inputs[0], &handle, &UA_TYPES[UA_TYPES_UINT32]);
  UA_Variant_setScalar(&inputs[1], &length, &UA_TYPES[UA_TYPES_INT32]);
  auto output = client_->callMethod(file_node_id_, read_method_id_, inputs);
  if (!output) {
    return std::unexpected{output.error()};
  }
  const auto values = output->values();
  if (values.size() != 1) {
    return std::unexpected{UA_STATUSCODE_BADTYPEMISMATCH};
  }
  // Some servers signal the end of the file with an empty variant instead of an empty byte string
  if (UA_Variant_isEmpty(&values[0])) {
    return {};
  }
  if (!UA_Variant_hasScalarType(&values[0], &UA_TYPES[UA_TYPES_BYTESTRING])) {
    return std::unexpected{UA_STATUSCODE_BADTYPEMISMATCH};
  }
  const auto* byte_string = static_cast<const UA_ByteString*>(values[0].data);
  if (byte_string->data == nullptr || byte_string->length == 0) {
    return {};
  }
  const auto* begin = reinterpret_cast<const std::byte*>(byte_string->data);
  return std::vector<std::byte>(begin, begin + byte_string->length);
}

UA_StatusCode RemoteFile::write(UA_UInt32 handle, std::span<const std::byte> data) {
  UA_ByteString byte_string;
  byte_string.length = data.size();
  byte_string.data = const_cast<UA_Byte*>(reinterpret_cast<const UA_Byte*>(data.data()));  // NOLINT(cppcoreguidelines-pro-type-const-cast)
  std::array<UA_Variant, 2> inputs{};
  UA_Variant_setScalar(&inputs[0], &handle, &UA_TYPES[UA_TYPES_UINT32]);
  UA_Variant_setScalar(&inputs[1], &byte_string, &UA_TYPES[UA_TYPES_BYTESTRING]);
  auto output = client_->callMethod(file_node_id_, write_method_id_, inputs);
  return output ? UA_STATUSCODE_GOOD : output.error();
}

std::optional<UA_UInt32> RemoteFile::maxByteStringLength() {
  auto property_node_id = client_->findChild(file_node_id_, "MaxByteStringLength", 0, UA_NS0ID_HASPROPERTY);
  if (!property_node_id) {
    return std::nullopt;
  }
  auto value = client_->readUInt32Value(*property_node_id);
  if (!value || *value == 0) {
    return std::nullopt;
  }
  return *value;
}

std::expected<NodeId, UA_StatusCode> createFileInDirectory(Client& client, const UA_NodeId& directory_node_id, std::string_view file_name) {
  auto method_id = client.findChild(directory_node_id, "CreateFile", 0, UA_NS0ID_HASCOMPONENT);
  if (!method_id) {
    return std::unexpected{method_id.error()};
  }

  std::string name(file_name);
  UA_String ua_file_name = UA_STRING(name.data());
  UA_Boolean request_file_open = false;
  std::array<UA_Variant, 2> inputs{};
  UA_Variant_setScalar(&inputs[0], &ua_file_name, &UA_TYPES[UA_TYPES_STRING]);
  UA_Variant_setScalar(&inputs[1], &request_file_open, &UA_TYPES[UA_TYPES_BOOLEAN]);
  auto output = client.callMethod(directory_node_id, *method_id, inputs);
  if (!output) {
    return std::unexpected{output.error()};
  }
  const auto values = output->values();
  if (values.empty() || !UA_Variant_hasScalarType(&values[0], &UA_TYPES[UA_TYPES_NODEID])) {
    return std::unexpected{UA_STATUSCODE_BADTYPEMISMATCH};
  }
  return NodeId::copyOf(*static_cast<const UA_NodeId*>(values[0].data));
}

}  // namespace org::apache::nifi::minifi::opc
