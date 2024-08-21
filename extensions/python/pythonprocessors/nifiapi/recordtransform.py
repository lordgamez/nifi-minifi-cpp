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

import traceback
import json
from abc import abstractmethod
from minifi_native import ProcessContext, ProcessSession, Processor
from .processorbase import ProcessorBase, WriteCallback
from .properties import FlowFile as FlowFileProxy
from .properties import ProcessContext as ProcessContextProxy
from .properties import PropertyDescriptor


class __RecordTransformResult__:
    class Java:
        implements = ['org.apache.nifi.python.processor.RecordTransformResult']

    def __init__(self, processor_result, recordJson):
        self.processor_result = processor_result
        self.recordJson = recordJson

    def getRecordJson(self):
        return self.recordJson

    def getSchema(self):
        return self.processor_result.schema

    def getRelationship(self):
        return self.processor_result.relationship

    def getPartition(self):
        return self.processor_result.partition


class RecordTransformResult:

    def __init__(self, record=None, schema=None, relationship="success", partition=None):
        self.record = record
        self.schema = schema
        self.relationship = relationship
        self.partition = partition

    def getRecord(self):
        return self.record

    def getSchema(self):
        return self.schema

    def getRelationship(self):
        return self.relationship

    def getPartition(self):
        return self.partition


class CachingAttributeMap:
    cache = None

    def __init__(self, delegate):
        self.delegate = delegate

    def getAttribute(self, attributeName):
        # Lazily initialize cache
        if self.cache is None:
            self.cache = {}
        if attributeName in self.cache:
            return self.cache[attributeName]

        value = self.delegate.getAttribute(attributeName)
        self.cache[attributeName] = value
        return value

    def getAttributes(self):
        return self.delegate.getAttributes()


class RecordTransform(ProcessorBase):
    RECORD_READER = PropertyDescriptor(
        name='Record Reader',
        display_name='Record Reader',
        description='''Specifies the Controller Service to use for reading incoming data''',
        required=True,
        controllerServiceDefinition='RecordSetReader'
    )
    RECORD_WRITER = PropertyDescriptor(
        name='Record Writer',
        display_name='Record Writer',
        description='''Specifies the Controller Service to use for writing out the records''',
        required=True,
        controllerServiceDefinition='RecordSetWriter',
    )

    def onInitialize(self, processor: Processor):
        super(RecordTransform, self).onInitialize(processor)
        processor.addProperty(self.RECORD_READER.name, self.RECORD_READER.description, None, self.RECORD_READER.required, False, False, None, self.RECORD_READER.controllerServiceDefinition)
        processor.addProperty(self.RECORD_WRITER.name, self.RECORD_WRITER.description, None, self.RECORD_WRITER.required, False, False, None, self.RECORD_WRITER.controllerServiceDefinition)

    def transformRecord(self, jsonarray, schema, attributemap):
        parsed_array = json.loads(jsonarray)
        results = self.arrayList()
        caching_attribute_map = CachingAttributeMap(attributemap)

        for record in parsed_array:
            result = self.transform(self.process_context, record, schema, caching_attribute_map)
            result_record = result.getRecord()
            resultjson = None if result_record is None else json.dumps(result_record)
            results.add(__RecordTransformResult__(result, resultjson))

        return results

    def onTrigger(self, context: ProcessContext, session: ProcessSession):
        original_flow_file = session.get()
        if not original_flow_file:
            return

        record_reader = context.getProperty(self.RECORD_READER).asControllerService()
        record_writer = context.getProperty(self.RECORD_WRITER).asControllerService()

        # flow_file = session.clone(original_flow_file)

        # flow_file_proxy = FlowFileProxy(session, flow_file)
        # context_proxy = ProcessContextProxy(context, self)
        # try:
        #     result = self.transform(context_proxy, flow_file_proxy)
        # except Exception:
        #     self.logger.error("Failed to transform flow file due to error:\n{}".format(traceback.format_exc()))
        #     session.remove(flow_file)
        #     session.transfer(original_flow_file, self.REL_FAILURE)
        #     return

        # if result.getRelationship() == "original":
        #     session.remove(flow_file)
        #     self.logger.error("Result relationship cannot be 'original', it is reserved for the original flow file, and transferred automatically in non-failure cases.")
        #     session.transfer(original_flow_file, self.REL_FAILURE)
        #     return

        # result_attributes = result.getAttributes()
        # if result.getRelationship() == "failure":
        #     session.remove(flow_file)
        #     if result_attributes is not None:
        #         for name, value in result_attributes.items():
        #             original_flow_file.setAttribute(name, value)
        #     if result.getContents() is not None:
        #         self.logger.error("'failure' relationship should not have content, the original flow file will be transferred automatically in this case.")
        #     session.transfer(original_flow_file, self.REL_FAILURE)
        #     return

        # if result_attributes is not None:
        #     for name, value in result_attributes.items():
        #         flow_file.setAttribute(name, value)

        # result_content = result.getContents()
        # if result_content is not None:
        #     session.write(flow_file, WriteCallback(result_content))

        # if result.getRelationship() == "success":
        #     session.transfer(flow_file, self.REL_SUCCESS)
        # else:
        #     session.transferToCustomRelationship(flow_file, result.getRelationship())
        session.transfer(original_flow_file, self.REL_ORIGINAL)

    @abstractmethod
    def transform(self, context: ProcessContextProxy, flowFile: FlowFileProxy) -> RecordTransformResult:
        pass
