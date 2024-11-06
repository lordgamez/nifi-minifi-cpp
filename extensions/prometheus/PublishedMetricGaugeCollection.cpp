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
#include "PublishedMetricGaugeCollection.h"

#include <utility>
#include <algorithm>
#include <unordered_set>

#include "prometheus/client_metric.h"
#include "state/PublishedMetricProvider.h"
#include "range/v3/range/conversion.hpp"
#include "range/v3/view/transform.hpp"
#include "state/nodes/MetricsBase.h"

namespace org::apache::nifi::minifi::extensions::prometheus {

PublishedMetricGaugeCollection::PublishedMetricGaugeCollection(const std::vector<gsl::not_null<std::shared_ptr<state::PublishedMetricProvider>>>& metric_providers, std::string agent_identifier)
  : metric_providers_{metric_providers},
    agent_identifier_(std::move(agent_identifier)) {
}

std::vector<::prometheus::MetricFamily> PublishedMetricGaugeCollection::Collect() const {
  std::vector<::prometheus::MetricFamily> collection;
  std::unordered_set<std::string> seen_metrics_names;
  for (const auto& metric_provider : metric_providers_) {
    logger_->log_info("Collecting metrics from provider: {}", static_cast<minifi::state::response::ResponseNode*>(metric_provider.get())->getName());
    for (const auto& metric : metric_provider->calculateMetrics()) {
      ::prometheus::ClientMetric client_metric;
      client_metric.label = ranges::views::transform(metric.labels, [](auto&& kvp) { return ::prometheus::ClientMetric::Label{kvp.first, kvp.second}; })
        | ranges::to<std::vector<::prometheus::ClientMetric::Label>>;
      client_metric.label.push_back(::prometheus::ClientMetric::Label{"agent_identifier", agent_identifier_});
      client_metric.gauge = ::prometheus::ClientMetric::Gauge{metric.value};
      logger_->log_info("Collecting metric: {} with value: {}", metric.name, metric.value);
      if (!seen_metrics_names.contains(metric.name)) {
        collection.push_back({
          .name = "minifi_" + metric.name,
          .help = "",
          .type = ::prometheus::MetricType::Gauge,
          .metric = { std::move(client_metric) }
        });
        seen_metrics_names.insert(metric.name);
        logger_->log_info("Added metric: {}", metric.name);
      } else {
        auto existing_metric = std::find_if(collection.begin(), collection.end(), [&](const auto& metric_family) { return metric_family.name == "minifi_" + metric.name; });
        if (existing_metric != collection.end()) {
          existing_metric->metric.push_back(std::move(client_metric));
        }
        logger_->log_info("Updated metric: {}", metric.name);
      }

    }
  }

  return collection;
}

}  // namespace org::apache::nifi::minifi::extensions::prometheus
