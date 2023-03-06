/**
 *
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
#include <memory>
#include <thread>

#include "../TestBase.h"
#include "../Catch.h"
#include "core/state/LogMetricsPublisher.h"
#include "core/state/nodes/ResponseNodeLoader.h"
#include "core/RepositoryFactory.h"
#include "utils/IntegrationTestUtils.h"

using namespace std::literals::chrono_literals;

namespace org::apache::nifi::minifi::test {

class LogPublisherTestFixture {
 public:
  LogPublisherTestFixture()
    : configuration_(std::make_shared<Configure>()),
      provenance_repo_(core::createRepository("provenancerepository", "provenancerepository")),
      flow_file_repo_(core::createRepository("flowfilerepository", "flowfilerepository")),
      response_node_loader_(std::make_shared<state::response::ResponseNodeLoader>(configuration_, provenance_repo_, flow_file_repo_, nullptr)),
      publisher_("LogMetricsPublisher") {
  }

 protected:
  std::shared_ptr<Configure> configuration_;
  std::shared_ptr<core::Repository> provenance_repo_;
  std::shared_ptr<core::Repository> flow_file_repo_;
  std::shared_ptr<state::response::ResponseNodeLoader> response_node_loader_;
  minifi::state::LogMetricsPublisher publisher_;
};

TEST_CASE_METHOD(LogPublisherTestFixture, "Logging interval property is mandatory", "[LogMetricsPublisher]") {
  LogTestController::getInstance().setTrace<minifi::state::LogMetricsPublisher>();
  SECTION("No logging interval is set") {
    REQUIRE_THROWS_WITH(publisher_.initialize(configuration_, response_node_loader_), "General Operation: Metrics logging interval not configured for log metrics publisher!");
  }
  SECTION("Logging interval is set to 2 seconds") {
    configuration_->set(minifi::Configuration::nifi_metrics_publisher_log_metrics_logging_interval, "2s");
    using org::apache::nifi::minifi::utils::verifyLogLinePresenceInPollTime;
    publisher_.initialize(configuration_, response_node_loader_);
    REQUIRE(verifyLogLinePresenceInPollTime(5s, "Metric logging interval is set to 2000 milliseconds"));
  }
}

TEST_CASE_METHOD(LogPublisherTestFixture, "Verify empty metrics if no valid metrics are defined", "[LogMetricsPublisher]") {
  LogTestController::getInstance().setTrace<minifi::state::LogMetricsPublisher>();
  configuration_->set(minifi::Configuration::nifi_metrics_publisher_log_metrics_logging_interval, "100ms");
  SECTION("No metrics are defined") {}
  SECTION("Only invalid metrics are defined") {
    configuration_->set(Configure::nifi_metrics_publisher_metrics, "InvalidMetric,NotValidMetricNode");
  }
  publisher_.initialize(configuration_, response_node_loader_);
  publisher_.loadMetricNodes();
  using org::apache::nifi::minifi::utils::verifyLogLinePresenceInPollTime;
  std::string expected_log = R"([info] {"LogMetrics":{}})";
  REQUIRE(verifyLogLinePresenceInPollTime(5s, expected_log));
}

TEST_CASE_METHOD(LogPublisherTestFixture, "Verify multiple metric nodes in logs", "[LogMetricsPublisher]") {
  LogTestController::getInstance().setTrace<minifi::state::LogMetricsPublisher>();
  configuration_->set(minifi::Configuration::nifi_metrics_publisher_log_metrics_logging_interval, "100ms");
  configuration_->set(Configure::nifi_metrics_publisher_metrics, "RepositoryMetrics,DeviceInfoNode");
  publisher_.initialize(configuration_, response_node_loader_);
  publisher_.loadMetricNodes();
  using org::apache::nifi::minifi::utils::verifyLogLinePresenceInPollTime;
  std::string expected_log = R"([info] {"LogMetrics":{"RepositoryMetrics":{"flowfilerepository":{"running":"false","full":"false","size":"0"},"provenancerepository")"
    R"(:{"running":"false","full":"false","size":"0"}},"deviceInfo":{"identifier":)";
  REQUIRE(verifyLogLinePresenceInPollTime(5s, expected_log));
}

TEST_CASE_METHOD(LogPublisherTestFixture, "Verify reloading different metrics", "[LogMetricsPublisher]") {
  LogTestController::getInstance().setTrace<minifi::state::LogMetricsPublisher>();
  configuration_->set(minifi::Configuration::nifi_metrics_publisher_log_metrics_logging_interval, "100ms");
  configuration_->set(Configure::nifi_metrics_publisher_metrics, "RepositoryMetrics");
  publisher_.initialize(configuration_, response_node_loader_);
  publisher_.loadMetricNodes();
  using org::apache::nifi::minifi::utils::verifyLogLinePresenceInPollTime;
  std::string expected_log = R"([info] {"LogMetrics":{"RepositoryMetrics":{"flowfilerepository":{"running":"false","full":"false","size":"0"},"provenancerepository")"
    R"(:{"running":"false","full":"false","size":"0"}}})";
  REQUIRE(verifyLogLinePresenceInPollTime(5s, expected_log));
  LogTestController::getInstance().reset();
  LogTestController::getInstance().setTrace<minifi::state::LogMetricsPublisher>();
  publisher_.clearMetricNodes();
  expected_log = R"([info] {"LogMetrics":{}})";
  REQUIRE(verifyLogLinePresenceInPollTime(5s, expected_log));
  LogTestController::getInstance().reset();
  LogTestController::getInstance().setTrace<minifi::state::LogMetricsPublisher>();
  configuration_->set(Configure::nifi_metrics_publisher_metrics, "DeviceInfoNode");
  publisher_.loadMetricNodes();
  expected_log = R"([info] {"LogMetrics":{"deviceInfo":{"identifier":)";
  REQUIRE(verifyLogLinePresenceInPollTime(5s, expected_log));
}

TEST_CASE_METHOD(LogPublisherTestFixture, "Verify generic and publisher specific metric properties", "[LogMetricsPublisher]") {
  LogTestController::getInstance().setTrace<minifi::state::LogMetricsPublisher>();
  configuration_->set(minifi::Configuration::nifi_metrics_publisher_log_metrics_logging_interval, "100ms");
  SECTION("Only generic metrics are defined") {
    configuration_->set(Configure::nifi_metrics_publisher_metrics, "RepositoryMetrics");
  }
  SECTION("Only publisher specific metrics are defined") {
    configuration_->set(Configure::nifi_metrics_publisher_log_metrics_publisher_metrics, "RepositoryMetrics");
  }
  SECTION("If both generic and publisher specific metrics are defined the publisher specific metrics are used") {
    configuration_->set(Configure::nifi_metrics_publisher_log_metrics_publisher_metrics, "RepositoryMetrics");
    configuration_->set(Configure::nifi_metrics_publisher_metrics, "DeviceInfoNode");
  }
  publisher_.initialize(configuration_, response_node_loader_);
  publisher_.loadMetricNodes();
  using org::apache::nifi::minifi::utils::verifyLogLinePresenceInPollTime;
  std::string expected_log = R"([info] {"LogMetrics":{"RepositoryMetrics":{"flowfilerepository":{"running":"false","full":"false","size":"0"},"provenancerepository")"
    R"(:{"running":"false","full":"false","size":"0"}}})";
  REQUIRE(verifyLogLinePresenceInPollTime(5s, expected_log));
}

TEST_CASE_METHOD(LogPublisherTestFixture, "Verify changing log level property for logging", "[LogMetricsPublisher]") {
  LogTestController::getInstance().setTrace<minifi::state::LogMetricsPublisher>();
  configuration_->set(minifi::Configuration::nifi_metrics_publisher_log_metrics_logging_interval, "100ms");
  configuration_->set(minifi::Configuration::nifi_metrics_publisher_log_metrics_log_level, "dEbUg");
  configuration_->set(Configure::nifi_metrics_publisher_metrics, "RepositoryMetrics");
  publisher_.initialize(configuration_, response_node_loader_);
  publisher_.loadMetricNodes();
  using org::apache::nifi::minifi::utils::verifyLogLinePresenceInPollTime;
  std::string expected_log = R"([debug] {"LogMetrics":{"RepositoryMetrics":{"flowfilerepository":{"running":"false","full":"false","size":"0"},"provenancerepository")"
    R"(:{"running":"false","full":"false","size":"0"}}})";
  REQUIRE(verifyLogLinePresenceInPollTime(5s, expected_log));
}

}  // namespace org::apache::nifi::minifi::test
