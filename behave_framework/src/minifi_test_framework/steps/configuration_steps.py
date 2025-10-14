#
#  Licensed to the Apache Software Foundation (ASF) under one or more
#  contributor license agreements.  See the NOTICE file distributed with
#  this work for additional information regarding copyright ownership.
#  The ASF licenses this file to You under the Apache License, Version 2.0
#  (the "License"); you may not use this file except in compliance with
#  the License.  You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
#  Unless required by applicable law or agreed to in writing, software
#  distributed under the License is distributed on an "AS IS" BASIS,
#  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#  See the License for the specific language governing permissions and
#  limitations under the License.
#

from behave import step

from minifi_test_framework.core.minifi_test_context import MinifiTestContext


@step('MiNiFi configuration "{config_key}" is set to "{config_value}"')
def step_impl(context: MinifiTestContext, config_key: str, config_value: str):
    context.minifi_container.set_property(config_key, config_value)


@step("log metrics publisher is enabled in MiNiFi")
def step_impl(context):
    context.minifi_container.set_property("nifi.metrics.publisher.LogMetricsPublisher.metrics", "RepositoryMetrics")
    context.minifi_container.set_property("nifi.metrics.publisher.LogMetricsPublisher.logging.interval", "1s")
    context.minifi_container.set_property("nifi.metrics.publisher.class", "LogMetricsPublisher")


@step('log property "{log_property_key}" is set to "{log_property_value}"')
def step_impl(context: MinifiTestContext, log_property_key: str, log_property_value: str):
    context.minifi_container.set_log_property(log_property_key, log_property_value)
