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

#include "XMLReader.h"

#include "core/Resource.h"
#include "utils/TimeUtil.h"

namespace org::apache::nifi::minifi::standard {

namespace {
bool hasChildNodes(const pugi::xml_node& node) {
  for (pugi::xml_node child : node.children()) {
    if (child.type() == pugi::node_element) {
      return true;
    }
  }
  return false;
}
}  // namespace

void XMLReader::addRecordFieldToObject(core::RecordObject& record_object, const std::string& name, const core::RecordField& field) const {
  auto it = record_object.find(name);
  if (it != record_object.end()) {
    if (std::holds_alternative<core::RecordArray>(it->second.value_)) {
      std::get<core::RecordArray>(it->second.value_).emplace_back(field);
    } else {
      core::RecordArray array;
      array.emplace_back(it->second);
      array.emplace_back(field);
      it->second = core::RecordField(std::move(array));
    }
  } else {
    record_object.emplace(name, field);
  }
}

void XMLReader::writeRecordField(core::RecordObject& record_object, const std::string& name, const std::string& value, bool override_content_field) const {
  if (!override_content_field && name == field_name_for_content_) {
    // If the name is the field name for content, we should not add it to the RecordObject, as it is a default tag for XML nodes.
    return;
  }
  if (value == "true" || value == "false") {
    addRecordFieldToObject(record_object, name, core::RecordField(value == "true"));
    return;
  } else if (auto date = utils::timeutils::parseDateTimeStr(value)) {
    addRecordFieldToObject(record_object, name, core::RecordField(*date));
    return;
  } else if (auto date = utils::timeutils::parseRfc3339(value)) {
    addRecordFieldToObject(record_object, name, core::RecordField(*date));
    return;
  }

  if (std::all_of(value.begin(), value.end(), ::isdigit)) {
    try {
      uint64_t value_as_uint64 = std::stoull(value);
      addRecordFieldToObject(record_object, name, core::RecordField(value_as_uint64));
      return;
    } catch (const std::exception&) {
    }
  }

  if (value.starts_with('-') && std::all_of(value.begin() + 1, value.end(), ::isdigit)) {
    try {
      int64_t value_as_int64 = std::stoll(value);
      addRecordFieldToObject(record_object, name, core::RecordField(value_as_int64));
      return;
    } catch (const std::exception&) {
    }
  }

  try {
    auto value_as_double = std::stod(value);
    addRecordFieldToObject(record_object, name, core::RecordField(value_as_double));
    return;
  } catch (const std::exception&) {
  }

  addRecordFieldToObject(record_object, name, core::RecordField(value));
}

void XMLReader::writeRecordFieldFromXmlNode(core::RecordObject& record_object, const pugi::xml_node& node) const {
  writeRecordField(record_object, node.name(), node.child_value());
}

void XMLReader::parseXmlNode(core::RecordObject& record_object, const pugi::xml_node& node) const {
  std::string pc_data_value;
  for (pugi::xml_node child : node.children()) {
    if (child.type() == pugi::node_element) {
      if (parse_xml_attributes_ && child.first_attribute()) {
        core::RecordObject child_record_object;
        for (const pugi::xml_attribute& attr : child.attributes()) {
          writeRecordField(child_record_object, attr.name(), attr.value());
        }
        parseXmlNode(child_record_object, child);
        record_object.emplace(child.name(), core::RecordField(std::move(child_record_object)));
      } else {
        if (hasChildNodes(child)) {
          core::RecordObject child_record_object;
          parseXmlNode(child_record_object, child);
          record_object.emplace(child.name(), core::RecordField(std::move(child_record_object)));
        } else {
          writeRecordFieldFromXmlNode(record_object, child);
        }
      }
    } else if (child.type() == pugi::node_pcdata) {
      pc_data_value.append(child.value());
    }
  }

  if (!pc_data_value.empty()) {
    writeRecordField(record_object, field_name_for_content_, pc_data_value, true);
  }
}

bool XMLReader::parseRecordsFromXml(core::RecordSet& record_set, const std::string& xml_content) const {
  pugi::xml_document doc;
  if (!doc.load_string(xml_content.c_str())) {
    return false;
  }

  pugi::xml_node root = doc.first_child();
  if (!root.first_child()) {
    return true;
  }

  core::RecordObject record_object;
  parseXmlNode(record_object, root);
  core::Record record(std::move(record_object));
  record_set.emplace_back(std::move(record));
  return true;
}

void XMLReader::onEnable() {
  field_name_for_content_ = getProperty(FieldNameForContent.name).value_or("value");
  parse_xml_attributes_ = getProperty(ParseXMLAttributes.name).value_or("false") == "true";
}

nonstd::expected<core::RecordSet, std::error_code> XMLReader::read(io::InputStream& input_stream) {
  core::RecordSet record_set{};
  const auto read_result = [this, &record_set](io::InputStream& input_stream) -> int64_t {
    std::string content;
    content.resize(input_stream.size());
    const auto read_ret = gsl::narrow<int64_t>(input_stream.read(as_writable_bytes(std::span(content))));
    if (io::isError(read_ret)) {
      return -1;
    }
    if (!parseRecordsFromXml(record_set, content)) {
      return -1;
    }
    return read_ret;
  }(input_stream);
  if (io::isError(read_result))
    return nonstd::make_unexpected(std::make_error_code(std::errc::invalid_argument));
  return record_set;
}

REGISTER_RESOURCE(XMLReader, ControllerService);
}  // namespace org::apache::nifi::minifi::standard
