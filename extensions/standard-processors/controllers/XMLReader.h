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
#pragma once

#include "pugixml.hpp"

#include "controllers/RecordSetReader.h"
#include "core/PropertyDefinitionBuilder.h"

namespace org::apache::nifi::minifi::standard {

class XMLReader final : public core::RecordSetReaderImpl {
 public:
  explicit XMLReader(const std::string_view name, const utils::Identifier& uuid = {}) : RecordSetReaderImpl(name, uuid) {}

  XMLReader(XMLReader&&) = delete;
  XMLReader(const XMLReader&) = delete;
  XMLReader& operator=(XMLReader&&) = delete;
  XMLReader& operator=(const XMLReader&) = delete;

  ~XMLReader() override = default;

  EXTENSIONAPI static constexpr const char* Description = "Reads XML content and creates Record objects. Records are expected in the second level of XML data, embedded in an enclosing root tag.";

  EXTENSIONAPI static constexpr auto FieldNameForContent = core::PropertyDefinitionBuilder<>::createProperty("Field Name for Content")
      .withDescription("If tags with content (e. g. <field>content</field>) are defined as nested records in the schema, the name of the tag will be used as name for the record and the value of "
                       "this property will be used as name for the field. If the tag contains subnodes besides the content (e.g. <field>content<subfield>subcontent</subfield></field>), "
                       "we need to define a name for the text content, so that it can be distinguished from the subnodes. If this property is not set, the default name 'value' will be used "
                       "for the text content of the tag in this case.")
      .build();
  EXTENSIONAPI static constexpr auto Properties = std::array<core::PropertyReference, 1>{FieldNameForContent};

  EXTENSIONAPI static constexpr bool SupportsDynamicProperties = false;
  ADD_COMMON_VIRTUAL_FUNCTIONS_FOR_CONTROLLER_SERVICES

  nonstd::expected<core::RecordSet, std::error_code> read(io::InputStream& input_stream) override;

  void initialize() override {
    setSupportedProperties(Properties);
  }
  void onEnable() override;
  void yield() override {}
  bool isRunning() const override { return getState() == core::controller::ControllerServiceState::ENABLED; }
  bool isWorkAvailable() override { return false; }

 private:
  void addRecordFieldToObject(core::RecordObject& record_object, const std::string& name, const core::RecordField& field) const;
  void writeRecordFieldFromXmlNode(core::RecordObject& record_object, const pugi::xml_node& node) const;
  void parseXmlNode(core::RecordObject& record_object, const pugi::xml_node& node) const;
  bool parseRecordsFromXml(core::RecordSet& record_set, const std::string& xml_content) const;

  std::string field_name_for_content_;
};

}  // namespace org::apache::nifi::minifi::standard
