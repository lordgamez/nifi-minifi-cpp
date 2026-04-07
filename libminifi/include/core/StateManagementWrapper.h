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
#pragma once

#include <mutex>

#include "core/logging/LoggerFactory.h"
#include "minifi-cpp/core/StateStorage.h"
#include "minifi-cpp/core/StateManager.h"
#include "core/controller/ControllerServiceProvider.h"
#include "minifi-cpp/properties/Configure.h"

namespace org::apache::nifi::minifi::core {

class StateManagementWrapper {
 public:
  StateManagementWrapper(controller::ControllerServiceProvider* controller_service_provider, const std::shared_ptr<minifi::Configure>& configuration);
  StateManager* getStateManager(const CoreComponent& component);
  bool hasStateManager() const { return state_manager_ != nullptr; }

  static std::shared_ptr<core::StateStorage> getStateStorage(const std::shared_ptr<logging::Logger>& logger, controller::ControllerServiceProvider* const controller_service_provider,
    const std::shared_ptr<minifi::Configure>& configuration);
  static std::shared_ptr<core::StateStorage> getOrCreateDefaultStateStorage(
    controller::ControllerServiceProvider* controller_service_provider, const std::shared_ptr<minifi::Configure>& configuration);

 private:
  static constexpr char const* DefaultStateStorageName = "defaultstatestorage";

  std::mutex state_manager_mutex_;
  std::shared_ptr<logging::Logger> logger_{logging::LoggerFactory<StateManagementWrapper>::getLogger()};
  std::shared_ptr<core::StateStorage> state_storage_;
  std::unique_ptr<StateManager> state_manager_;
};

}  // namespace org::apache::nifi::minifi::core
