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
import docker
import logging
from pathlib import Path
from minifi_test_framework.containers.docker_image_builder import DockerImageBuilder
from minifi_test_framework.core.hooks import common_before_scenario
from minifi_test_framework.core.hooks import common_after_scenario
from containers.prometheus_container import PrometheusContainer


def before_all(context):
    check_log_lines_path = Path(__file__).resolve().parent / "resources" / "prometheus_checker.py"
    check_log_lines_content = None
    with open(check_log_lines_path, "rb") as f:
        check_log_lines_content = f.read()
    dockerfile = """
        FROM python:3.13-slim-bookworm
        RUN pip install requests prometheus-api-client==0.7.0
        COPY prometheus_checker.py /scripts/prometheus_checker.py"""
    prometheus_helper_builder = DockerImageBuilder(
        image_tag="minifi-prometheus-helper:latest",
        dockerfile_content=dockerfile,
        files_on_context={"prometheus_checker.py": check_log_lines_content}
    )
    prometheus_helper_builder.build()
    # Pre-pull the Prometheus image before any scenario starts. Without this, docker.containers.run(detach=True)
    # blocks for the entire image pull duration inside Container.deploy(). If the pull takes longer than MiNiFi's
    # RocksDB compaction period (default 120s), the repositories will already be empty when Prometheus first
    # scrapes, causing _verify_metric_larger_than_zero('minifi_repository_size_bytes') to fail.
    logging.info(f"Pre-pulling {PrometheusContainer.IMAGE} image...")
    docker.from_env().images.pull(PrometheusContainer.IMAGE)


def before_scenario(context, scenario):
    common_before_scenario(context, scenario)


def after_scenario(context, scenario):
    common_after_scenario(context, scenario)
