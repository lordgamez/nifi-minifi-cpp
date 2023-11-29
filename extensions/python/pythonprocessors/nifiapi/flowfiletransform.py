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
from minifi_native import OutputStream, Processor, ProcessContext, ProcessSession


class WriteCallback:
    def __init__(self, content):
        self.content = content

    def process(self, output_stream: OutputStream):
        output_stream.write(self.content)
        return len(self.content)


class FlowFileTransformResult:
    def __init__(self, relationship: str, attributes=None, contents=None):
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


class FlowFileTransform(ABC):
    # These will be added through the python bindings using C API
    logger = None
    REL_SUCCESS = None
    REL_FAILURE = None
    REL_ORIGINAL = None

    def describe(self, processor: Processor):
        processor.setDescription(self.ProcessorDetails.description)

    def onInitialize(self, processor: Processor):
        processor.setSupportsDynamicProperties()
        for property in self.getPropertyDescriptors():
            validator = translateStandardValidatorToMiNiFiPropertype(property.validators)
            expression_language_supported = True if property.expressionLanguageScope != ExpressionLanguageScope.NONE else False
            processor.addProperty(property.name, property.description, property.defaultValue, property.required, expression_language_supported, validator)

    def onTrigger(self, context: ProcessContext, session: ProcessSession):
        original_flow_file = session.get()
        if not original_flow_file:
            return

        flow_file = session.clone(original_flow_file)

        flow_file_proxy = FlowFileProxy(session, flow_file)
        context_proxy = ProcessContextProxy(context)
        try:
            result = self.transform(context_proxy, flow_file_proxy)
        except Exception as e:
            self.logger.error("Failed to transform flow file due to error: {}".format(str(e)))
            session.remove(flow_file)
            session.transfer(original_flow_file, self.REL_FAILURE)
            return

        if result.getRelationship() == "failure":
            session.remove(flow_file)
            session.transfer(original_flow_file, self.REL_FAILURE)
            return

        result_attributes = result.getAttributes()
        if result_attributes is not None:
            for attribute in result_attributes:
                flow_file.addAttribute(attribute, result_attributes[attribute])

        result_content = result.getContents()
        if result_content is not None:
            session.write(flow_file, WriteCallback(result_content))

        session.transfer(flow_file, self.REL_SUCCESS)
        session.transfer(original_flow_file, self.REL_ORIGINAL)

    @abstractmethod
    def transform(self, context: ProcessContextProxy, flowFile: FlowFileProxy) -> FlowFileTransformResult:
        pass