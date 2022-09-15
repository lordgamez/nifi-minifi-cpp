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
#pragma once

#include <string>
#include <chrono>
#include <atomic>
#include <unordered_map>
#include <mutex>
#include <vector>

#include "core/state/nodes/MetricsBase.h"
#include "core/state/PublishedMetricProvider.h"
#include "utils/Averager.h"

namespace org::apache::nifi::minifi::core {

class Processor;

class ProcessorMetrics : public state::response::ResponseNode {
 public:
  explicit ProcessorMetrics(const Processor& source_processor);

  [[nodiscard]] std::string getName() const override;

  std::vector<state::response::SerializedResponseNode> serialize() override;
  std::vector<state::PublishedMetric> calculateMetrics() override;
  void incrementRelationshipTransferCount(const std::string& relationship);
  std::chrono::milliseconds getAverageOnTriggerRuntime() const;
  std::chrono::milliseconds getLastOnTriggerRuntime() const;
  void addLastOnTriggerRuntime(std::chrono::milliseconds runtime);

  std::atomic<size_t> iterations{0};
  std::atomic<size_t> transferred_flow_files{0};
  std::atomic<uint64_t> transferred_bytes{0};

 protected:
  [[nodiscard]] std::unordered_map<std::string, std::string> getCommonLabels() const;
  static const uint8_t STORED_ON_TRIGGER_RUNTIME_COUNT = 10;

  std::mutex transferred_relationships_mutex_;
  std::unordered_map<std::string, size_t> transferred_relationships_;
  const Processor& source_processor_;
  utils::Averager<std::chrono::milliseconds> on_trigger_runtime_averager_;
};

}  // namespace org::apache::nifi::minifi::core
