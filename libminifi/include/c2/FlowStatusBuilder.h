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

#include <vector>
#include <mutex>

#include "FlowStatusRequest.h"
#include "rapidjson/rapidjson.h"
#include "core/ProcessGroup.h"
#include "core/BulletinStore.h"
#include "core/logging/LoggerFactory.h"

namespace org::apache::nifi::minifi::c2 {

class FlowStatusBuilder {
 public:
  void setRoot(core::ProcessGroup* root);
  void setBulletinStore(core::BulletinStore* bulletin_store);
  rapidjson::Document buildFlowStatus(const std::vector<FlowStatusRequest>& requests);

 private:
  void addProcessorStatus(core::Processor* processor, rapidjson::Value& processor_status_list, rapidjson::Document::AllocatorType& allocator, const std::unordered_set<std::string>& options);
  nonstd::expected<void, std::string> addProcessorStatuses(rapidjson::Value& processor_status_list, rapidjson::Document::AllocatorType& allocator,
    const std::string& identifier, const std::unordered_set<std::string>& options);
  void addConnectionStatus(Connection* connection, rapidjson::Value& processor_status_list, rapidjson::Document::AllocatorType& allocator, const std::unordered_set<std::string>& options);
  nonstd::expected<void, std::string> addConnectionStatuses(rapidjson::Value& processor_status_list, rapidjson::Document::AllocatorType& allocator,
    const std::string& identifier, const std::unordered_set<std::string>& options);

  std::mutex root_mutex_;
  core::ProcessGroup* root_{};
  core::BulletinStore* bulletin_store_{};
  std::shared_ptr<core::logging::Logger> logger_ = core::logging::LoggerFactory<FlowStatusBuilder>::getLogger();
};

}  // namespace org::apache::nifi::minifi::c2
