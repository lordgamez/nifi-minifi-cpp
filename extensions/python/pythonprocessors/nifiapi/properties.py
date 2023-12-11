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

from enum import Enum
from minifi_native import DataConverter
from typing import List


class ExpressionLanguageScope(Enum):
    NONE = 1
    ENVIRONMENT = 2
    FLOWFILE_ATTRIBUTES = 3


class ValidatorGenerator:
    def createNonNegativeFloatingPointValidator(self, *args):
        return StandardValidators.ALWAYS_VALID

    def createDirectoryExistsValidator(self, *args):
        return StandardValidators.ALWAYS_VALID

    def createURLValidator(self, *args):
        return StandardValidators.ALWAYS_VALID

    def createListValidator(self, *args):
        return StandardValidators.ALWAYS_VALID

    def createTimePeriodValidator(self, *args):
        return StandardValidators.TIME_PERIOD_VALIDATOR

    def createAttributeExpressionLanguageValidator(self, *args):
        return StandardValidators.ALWAYS_VALID

    def createDataSizeBoundsValidator(self, *args):
        return StandardValidators.DATA_SIZE_VALIDATOR

    def createRegexMatchingValidator(self, *args):
        return StandardValidators.ALWAYS_VALID

    def createRegexValidator(self, *args):
        return StandardValidators.ALWAYS_VALID

    def createLongValidator(self, *args):
        return StandardValidators.LONG_VALIDATOR


class StandardValidators:
    _standard_validators = ValidatorGenerator()

    ALWAYS_VALID = 0
    NON_EMPTY_VALIDATOR = 1
    INTEGER_VALIDATOR = 2
    POSITIVE_INTEGER_VALIDATOR = 3
    POSITIVE_LONG_VALIDATOR = 4
    NON_NEGATIVE_INTEGER_VALIDATOR = 5
    NUMBER_VALIDATOR = 6
    LONG_VALIDATOR = 7
    PORT_VALIDATOR = 8
    NON_EMPTY_EL_VALIDATOR = 9
    HOSTNAME_PORT_LIST_VALIDATOR = 10
    BOOLEAN_VALIDATOR = 11
    URL_VALIDATOR = 12
    URI_VALIDATOR = 13
    REGULAR_EXPRESSION_VALIDATOR = 14
    REGULAR_EXPRESSION_WITH_EL_VALIDATOR = 15
    TIME_PERIOD_VALIDATOR = 16
    DATA_SIZE_VALIDATOR = 17
    FILE_EXISTS_VALIDATOR = 18
    NON_NEGATIVE_FLOATING_POINT_VALIDATOR = 19


class MinifiPropertyTypes:
    INTEGER_TYPE = 0
    LONG_TYPE = 1
    BOOLEAN_TYPE = 2
    DATA_SIZE_TYPE = 3
    TIME_PERIOD_TYPE = 4
    NON_BLANK_TYPE = 5
    PORT_TYPE = 6


def translateStandardValidatorToMiNiFiPropertype(validators: List[StandardValidators]) -> MinifiPropertyTypes:
    if validators is None or len(validators) == 0 or len(validators) > 1:
        return None

    validator = validators[0]
    if validator == StandardValidators.INTEGER_VALIDATOR:
        return MinifiPropertyTypes.INTEGER_TYPE
    if validator == StandardValidators.LONG_VALIDATOR:
        return MinifiPropertyTypes.LONG_TYPE
    if validator == StandardValidators.BOOLEAN_VALIDATOR:
        return MinifiPropertyTypes.BOOLEAN_TYPE
    if validator == StandardValidators.DATA_SIZE_VALIDATOR:
        return MinifiPropertyTypes.DATA_SIZE_TYPE
    if validator == StandardValidators.TIME_PERIOD_VALIDATOR:
        return MinifiPropertyTypes.TIME_PERIOD_TYPE
    if validator == StandardValidators.NON_EMPTY_VALIDATOR:
        return MinifiPropertyTypes.NON_BLANK_TYPE
    if validator == StandardValidators.PORT_VALIDATOR:
        return MinifiPropertyTypes.PORT_TYPE

    return None


class PropertyDependency:
    def __init__(self, property_descriptor, *dependent_values):
        if dependent_values is None:
            dependent_values = []

        self.property_descriptor = property_descriptor
        self.dependent_values = dependent_values


class ResourceDefinition:
    def __init__(self, allow_multiple=False, allow_file=True, allow_url=False, allow_directory=False, allow_text=False):
        self.allow_multiple = allow_multiple
        self.allow_file = allow_file
        self.allow_url = allow_url
        self.allow_directory = allow_directory
        self.allow_text = allow_text

    @staticmethod
    def from_java_definition(java_definition):
        if java_definition is None:
            return None

        allow_multiple = java_definition.getCardinality().name() == "MULTIPLE"
        resource_types = java_definition.getResourceTypes()
        allow_file = False
        allow_url = False
        allow_directory = False
        allow_text = False
        for type in resource_types:
            name = type.name()
            if name == "FILE":
                allow_file = True
            elif name == "DIRECTORY":
                allow_directory = True
            elif name == "TEXT":
                allow_text = True
            elif name == "URL":
                allow_url = True

        return ResourceDefinition(allow_multiple, allow_file, allow_url, allow_directory, allow_text)


class PropertyDescriptor:
    def __init__(self, name, description, required=False, sensitive=False,
                 display_name=None, default_value=None, allowable_values=None,
                 dependencies=None, expression_language_scope=ExpressionLanguageScope.NONE,
                 dynamic=False, validators=None,
                 resource_definition=None, controller_service_definition=None):
        """
        :param name: the name of the property
        :param description: a description of the property
        :param required: a boolean indicating whether or not the property is required. Defaults to False.
        :param sensitive: a boolean indicating whether or not the property is sensitive. Defaults to False.
        :param display_name: Once a Processor has been released, its properties' configuration are stored as key/value pairs where the key is the name of the property.
                             Because of that, subsequent versions of the Processor should not change the name of a property. However, there are times when renaming a property
                             would be advantageous. For example, one might find a typo in the name of a property, or users may find a property name confusing. While the name of
                             the property should not be changed, a display_name may be added. Doing this results in the Display Name being used in the NiFi UI but still maintains
                             the original name as the key. Generally, this value should be left unspecified. If unspecified (or a value of `None`), the display name will default
                             to whatever the name is. However, the display_name may be changed at any time between versions without adverse effects.
        :param default_value: a default value for the property. If not specified, the initial value will be unset. If specified, any time the value is removed, it is reset to this
                              default_value. That is to say, if a default value is specified, the property cannot be unset.
        :param allowable_values: a list of string values that are allowed. If specified, the UI will present the options as a drop-down instead of a freeform text. If any other value
                                 is specified for the property, the Processor will be invalid.
        :param dependencies: a list of dependencies for this property. By default, all properties are always configurable. However, sometimes we want to expose a property that only makes
                             sense in certain circumstances. In this situation, we can say that Property A depends on Property B. Now, Property A will only be shown if a value is selected
                             for Property B. Additionally, we may say that Property A depends on Property B being set to some explicit value, say "foo." Now, Property A will only be shown
                             in the UI if Property B is set to a value of "foo." If a Property is not shown in the UI, its value will also not be validated. For example, if we indicate that
                             Property A is required and depends on Property B, then Property A is only required if Property B is set.
        :param expression_language_scope: documents the scope in which Expression Language is valid. This value must be specified as one of the enum values
                                          in `nifiapi.properties.ExpressionLanguageScope`. A value of `NONE` indicates that Expression Language will not be evaluated for this property.
                                          This is the default. A value of `FLOWFILE_ATTRIBUTES` indicates that FlowFile attributes may be referenced when configuring the property value.
                                          A value of `ENVIRONMENT` indicates that Expression Language may be used and may reference environment variables but may not reference FlowFile attributes.
                                          For example, a value of `${now()}` might be used to reference the current date and time, or `${hostname(true)}` might be used to specify the hostname.
                                          Or a value of `${ENV_VAR}` could be used to reference an environment variable named `ENV_VAR`.
        :param dynamic: whether or not this Property Descriptor represents a dynamic (aka user-defined) property. This is not necessary to specify, as the framework can determine this.
                        However, it is available if there is a desire to explicitly set it for completeness' sake.
        :param validators: A list of property validators that can be used to ensure that the user-supplied value is valid. The standard validators can be referenced using the
                           members of the `nifiapi.properties.StandardValidators` class.
        :param resource_definition: an instance of `nifiapi.properties.ResourceDefinition`. This may be used to convey that the property references a file, directory, or URL, or a set of them.
        :param controller_service_definition: if this Processor is to make use of a Controller Service, this indicates the type of Controller Service. This will always be a fully-qualified
                                              classname of a Java interface that extends from `ControllerService`.
        """
        if validators is None:
            validators = [StandardValidators.ALWAYS_VALID]

        self.name = name
        self.description = description
        self.required = required
        self.sensitive = sensitive
        self.displayName = display_name
        self.defaultValue = default_value
        self.allowableValues = allowable_values
        self.dependencies = dependencies
        self.expressionLanguageScope = expression_language_scope
        self.dynamic = dynamic
        self.validators = validators
        self.resourceDefinition = resource_definition
        self.controllerServiceDefinition = controller_service_definition


class TimeUnit(Enum):
    NANOSECONDS = "NANOSECONDS",
    MICROSECONDS = "MICROSECONDS",
    MILLISECONDS = "MILLISECONDS",
    SECONDS = "SECONDS",
    MINUTES = "MINUTES",
    HOURS = "HOURS",
    DAYS = "DAYS"


class DataUnit(Enum):
    B = "B",
    KB = "KB",
    MB = "MB",
    GB = "GB",
    TB = "TB"


class FlowFileProxy:
    def __init__(self, session, flow_file):
        self.session = session
        self.flow_file = flow_file

    def getContentsAsBytes(self):
        return self.session.getContentsAsBytes(self.flow_file)


class PythonPropertyValue:
    def __init__(self, cpp_context, cpp_data_converter: DataConverter, name: str, string_value: str, el_supported: bool):
        self.cpp_context = cpp_context
        self.cpp_data_converter = cpp_data_converter
        self.value = None
        self.name = name
        if string_value is not None:
            self.value = string_value
        self.el_supported = el_supported

    def getValue(self):
        return self.value

    def isSet(self):
        return self.value is not None

    def asInteger(self):
        if not self.value:
            return None
        return int(self.value)

    def asBoolean(self):
        if not self.value:
            return None
        return self.value.lower() == 'true'

    def asFloat(self):
        if not self.value:
            return None
        return float(self.value)

    def asTimePeriod(self, time_unit):
        if not self.value:
            return None
        milliseconds = self.cpp_data_converter.timePeriodStringToMilliseconds(self.value)
        if time_unit == TimeUnit.NANOSECONDS:
            return milliseconds * 1000000
        if time_unit == TimeUnit.MICROSECONDS:
            return milliseconds * 1000
        if time_unit == TimeUnit.MILLISECONDS:
            return milliseconds
        if time_unit == TimeUnit.SECONDS:
            return int(round(milliseconds / 1000))
        if time_unit == TimeUnit.MINUTES:
            return int(round(milliseconds / 1000 / 60))
        if time_unit == TimeUnit.HOURS:
            return int(round(milliseconds / 1000 / 60 / 60))
        if time_unit == TimeUnit.DAYS:
            return int(round(milliseconds / 1000 / 60 / 60 / 24))
        return 0

    def asDataSize(self, data_unit):
        if not self.value:
            return None
        bytes = self.cpp_data_converter.dataSizeStringToBytes(self.value)
        if data_unit == DataUnit.B:
            return bytes
        if data_unit == DataUnit.KB:
            return int(bytes / 1000)
        if data_unit == DataUnit.MB:
            return int(bytes / 1000 / 1000)
        if data_unit == DataUnit.GB:
            return int(bytes / 1000 / 1000 / 1000)
        if data_unit == DataUnit.TB:
            return int(bytes / 1000 / 1000 / 1000 / 1000)
        return 0

    def evaluateAttributeExpressions(self, flow_file_proxy: FlowFileProxy):
        # If Expression Language is supported and present, evaluate it and return a new PropertyValue.
        # Otherwise just return self, in order to avoid the cost of making the call to cpp for getProperty
        if self.el_supported:
            new_string_value = self.cpp_context.getProperty(self.name, flow_file_proxy.flow_file)
            return PythonPropertyValue(self.cpp_context, self.cpp_data_converter, self.name, new_string_value, self.el_supported)

        return self


class ProcessContextProxy:
    def __init__(self, cpp_context):
        self.cpp_context = cpp_context
        self.cpp_data_converter = DataConverter()

    def getProperty(self, descriptor: PropertyDescriptor) -> PythonPropertyValue:
        property_value = self.cpp_context.getProperty(descriptor.name)
        return PythonPropertyValue(self.cpp_context, self.cpp_data_converter, descriptor.name, property_value, descriptor.expressionLanguageScope != ExpressionLanguageScope.NONE)
