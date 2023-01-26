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

#include "core/state/nodes/RepositoryMetrics.h"
#include "core/Resource.h"

namespace org::apache::nifi::minifi::state::response {

void RepositoryMetrics::addRepository(const std::shared_ptr<core::Repository> &repo) {
  if (nullptr != repo) {
    repositories_.insert(std::make_pair(repo->getName(), repo));
  }
}

std::vector<SerializedResponseNode> RepositoryMetrics::serialize() {
  std::vector<SerializedResponseNode> serialized;
  for (auto conn : repositories_) {
    auto repo = conn.second;
    SerializedResponseNode parent;
    parent.name = repo->getName();
    SerializedResponseNode datasize;
    datasize.name = "running";
    datasize.value = repo->isRunning();

    SerializedResponseNode datasizemax;
    datasizemax.name = "full";
    datasizemax.value = repo->isFull();

    SerializedResponseNode queuesize;
    queuesize.name = "size";
    queuesize.value = std::to_string(repo->getRepoSize());

    parent.children.push_back(datasize);
    parent.children.push_back(datasizemax);
    parent.children.push_back(queuesize);

    serialized.push_back(parent);
  }
  return serialized;
}

std::vector<PublishedMetric> RepositoryMetrics::calculateMetrics() {
  std::vector<PublishedMetric> metrics;
  for (const auto& [_, repo] : repositories_) {
    metrics.push_back({"is_running", (repo->isRunning() ? 1.0 : 0.0), {{"metric_class", getName()}, {"repository_name", repo->getName()}}});
    metrics.push_back({"is_full", (repo->isFull() ? 1.0 : 0.0), {{"metric_class", getName()}, {"repository_name", repo->getName()}}});
    metrics.push_back({"repository_size", static_cast<double>(repo->getRepoSize()), {{"metric_class", getName()}, {"repository_name", repo->getName()}}});
  }
  return metrics;
}

REGISTER_RESOURCE(RepositoryMetrics, DescriptionOnly);

}  // namespace org::apache::nifi::minifi::state::response

