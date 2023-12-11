# Licensed to the Apache Software Foundation (ASF) under one or more
# contributor license agreements.  See the NOTICE file distributed with
# this work for additional information regarding copyright ownership.
# The ASF licenses this file to You under the Apache License, Version 2.0
# (the "License"); you may not use this file except in compliance with
# the License.  You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

from abc import ABC, abstractmethod
from .properties import ExpressionLanguageScope, FlowFileProxy, ProcessContextProxy, translateStandardValidatorToMiNiFiPropertype


class WriteCallback:
    def __init__(self, content):
        self.content = content

    def process(self, output_stream):
        output_stream.write(self.content.encode('utf-8'))
        return len(self.content)


class FlowFileTransform(ABC):
    # These will be added through the python bindings using C API
    logger = None
    REL_SUCCESS = None
    REL_FAILURE = None

    def describe(self, processor):
        processor.setDescription(self.ProcessorDetails.description)

    def onInitialize(self, processor):
        processor.setSupportsDynamicProperties()
        for property in self.property_descriptors:
            validator = translateStandardValidatorToMiNiFiPropertype(property.validators)
            expression_language_supported = True if property.expressionLanguageScope != ExpressionLanguageScope.NONE else False
            processor.addProperty(property.name, property.description, property.defaultValue, property.required, expression_language_supported, validator)

    def onTrigger(self, context, session):
        flow_file = session.get()
        if not flow_file:
            return

        flow_file_proxy = FlowFileProxy(session, flow_file)
        context_proxy = ProcessContextProxy(context)
        result = self.transform(context_proxy, flow_file_proxy)

        result_content = result.getContents()
        if result_content is not None:
            session.write(flow_file, WriteCallback(str(result_content)))

        result_attributes = result.getAttributes()
        if result_attributes is not None:
            for attribute in result_attributes:
                flow_file.addAttribute(attribute, result_attributes[attribute])

        if result.getRelationship() == "success":
            session.transfer(flow_file, self.REL_SUCCESS)
        elif result.getRelationship() == "failure":
            session.transfer(flow_file, self.REL_FAILURE)  # TODO add original relationship

    @abstractmethod
    def transform(self, context, flowFile):
        pass


class FlowFileTransformResult:
    def __init__(self, relationship, attributes=None, contents=None):
        self.relationship = relationship
        self.attributes = attributes
        if contents is not None and isinstance(contents, str):
            self.contents = str.encode(contents)
        else:
            self.contents = contents

    def getRelationship(self):
        return self.relationship

    def getContents(self):
        return self.contents

    def getAttributes(self):
        return self.attributes