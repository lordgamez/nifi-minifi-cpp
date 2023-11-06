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

@ENABLE_GRAFANA_LOKI
Feature: MiNiFi can publish logs to Grafana Loki server

  Background:
    Given the content of "/tmp/output" is monitored

  Scenario: Logs are published to Loki server through REST API
    Given a Grafana Loki server is set up
    And a TailFile processor with the "File to Tail" property set to "/tmp/input/test_file.log"
    And a file with filename "test_file.log" and content "log line 1\nlog line 2\nlog line 3\n" is present in "/tmp/input"
    And a PushGrafanaLokiREST processor with the "Url" property set to "http://grafana-loki-server-${feature_id}:3100/"
    And the "Stream Labels" property of the PushGrafanaLokiREST processor is set to "job=minifi,id=docker-test"
    And the "success" relationship of the TailFile processor is connected to the PushGrafanaLokiREST
    When all instances start up
    Then "log line 1;log line 2;log line 3" lines are published to the Grafana Loki server in less than 60 seconds
