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


import logging
import os
import types
from behave.model import Scenario
from behave.runner import Context
from minifi_test_framework.core.minifi_test_context import MinifiTestContext

import docker


def get_minifi_container_image():
    if 'MINIFI_TAG_PREFIX' in os.environ and 'MINIFI_VERSION' in os.environ:
        minifi_tag_prefix = os.environ['MINIFI_TAG_PREFIX']
        minifi_version = os.environ['MINIFI_VERSION']
        return 'apacheminificpp:' + minifi_tag_prefix + minifi_version
    return "apacheminificpp:behave"

def inject_scenario_id(context, step):
    if "${scenario_id}" in step.name:
        step.name = step.name.replace("${scenario_id}", context.scenario_id)
    if step.table:
        for row in step.table:
            for i in range(len(row.cells)):
                if "${scenario_id}" in row.cells[i]:
                    row.cells[i] = row.cells[i].replace("${scenario_id}", context.scenario_id)


def common_before_scenario(context: Context, scenario: Scenario):
    if not hasattr(context, "minifi_container_image"):
        context.minifi_container_image = get_minifi_container_image()

    if not hasattr(context, "get_or_create_minifi_container"):
        context.get_or_create_minifi_container = types.MethodType(MinifiTestContext.get_or_create_minifi_container, context)

    if not hasattr(context, "get_or_create_default_minifi_container"):
        context.get_or_create_default_minifi_container = types.MethodType(MinifiTestContext.get_or_create_default_minifi_container, context)

    if not hasattr(context, "get_minifi_container"):
        context.get_minifi_container = types.MethodType(MinifiTestContext.get_minifi_container, context)

    if not hasattr(context, "get_default_minifi_container"):
        context.get_default_minifi_container = types.MethodType(MinifiTestContext.get_default_minifi_container, context)

    logging.info("Running scenario: %s", scenario)
    context.scenario_id = scenario.filename.rsplit("/", 1)[1].split(".")[0] + "-" + str(
        scenario.parent.scenarios.index(scenario))
    network_name = f"{context.scenario_id}-net"
    docker_client = docker.client.from_env()
    try:
        existing_network = docker_client.networks.get(network_name)
        logging.warning(f"Found existing network '{network_name}'. Removing it first.")
        existing_network.remove()
    except docker.errors.NotFound:
        pass  # No existing network found, which is good.
    context.network = docker_client.networks.create(network_name)
    context.containers = {}
    context.resource_dir = None

    for step in scenario.steps:
        inject_scenario_id(context, step)


def common_after_scenario(context: MinifiTestContext, scenario: Scenario):
    for container in context.containers.values():
        container.clean_up()
    context.network.remove()
