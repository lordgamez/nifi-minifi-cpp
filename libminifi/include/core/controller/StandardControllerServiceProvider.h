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

#include <string>
#include <utility>
#include <memory>
#include <vector>
#include <thread>
#include "core/ProcessGroup.h"
#include "core/ClassLoader.h"
#include "core/controller/ControllerService.h"
#include "ControllerServiceNodeMap.h"
#include "ControllerServiceNode.h"
#include "StandardControllerServiceNode.h"
#include "ControllerServiceProvider.h"
#include "core/logging/LoggerFactory.h"

using namespace std::literals::chrono_literals;

namespace org::apache::nifi::minifi::core::controller {

class StandardControllerServiceProvider : public ControllerServiceProviderImpl  {
 public:
  explicit StandardControllerServiceProvider(std::unique_ptr<ControllerServiceNodeMap> services, std::shared_ptr<Configure> configuration, ClassLoader& loader = ClassLoader::getDefaultClassLoader())
      : ControllerServiceProviderImpl(std::move(services)),
        extension_loader_(loader),
        configuration_(std::move(configuration)),
        admin_yield_duration_(readAdministrativeYieldDuration()),
        logger_(logging::LoggerFactory<StandardControllerServiceProvider>::getLogger()) {
  }

  StandardControllerServiceProvider(const StandardControllerServiceProvider &other) = delete;
  StandardControllerServiceProvider(StandardControllerServiceProvider &&other) = delete;

  StandardControllerServiceProvider& operator=(const StandardControllerServiceProvider &other) = delete;
  StandardControllerServiceProvider& operator=(StandardControllerServiceProvider &&other) = delete;
  ~StandardControllerServiceProvider() override {
    stopEnableRetryThread();
  }

  std::shared_ptr<ControllerServiceNode> createControllerService(const std::string& type, const std::string&, const std::string& id, bool) override {
    std::shared_ptr<ControllerService> new_controller_service = extension_loader_.instantiate<ControllerService>(type, id);

    if (!new_controller_service) {
      return nullptr;
    }

    std::shared_ptr<ControllerServiceNode> new_service_node = std::make_shared<StandardControllerServiceNode>(new_controller_service,
                                                                                                              sharedFromThis<ControllerServiceProvider>(), id,
                                                                                                              configuration_);

    controller_map_->put(id, new_service_node);
    return new_service_node;
  }

  void enableAllControllerServices() override {
    stopEnableRetryThread();
    {
      std::lock_guard<std::mutex> lock(enable_retry_mutex_);
      logger_->log_info("Enabling {} controller services", controller_map_->getAllControllerServices().size());
      for (const auto& service : controller_map_->getAllControllerServices()) {
        logger_->log_info("Enabling {}", service->getName());
        if (!service->canEnable()) {
          logger_->log_warn("Service {} cannot be enabled", service->getName());
          continue;
        }
        if (!service->enable()) {
          logger_->log_warn("Could not enable {}", service->getName());
          controller_services_to_enable_.push_back(service);
        }
      }
    }
    startEnableRetryThread();
  }

  void disableAllControllerServices() override {
    stopEnableRetryThread();
    logger_->log_info("Disabling {} controller services", controller_map_->getAllControllerServices().size());
    for (const auto& service : controller_map_->getAllControllerServices()) {
      logger_->log_info("Disabling {}", service->getName());
      if (!service->disable()) {
        logger_->log_warn("Could not disable {}", service->getName());
      }
    }
  }

  void clearControllerServices() override {
    stopEnableRetryThread();
    controller_map_->clear();
  }

 protected:
  void stopEnableRetryThread() {
    enable_retry_thread_running_ = false;
    enable_retry_condition_.notify_all();
    if (controller_service_enable_retry_thread_.joinable()) {
      controller_service_enable_retry_thread_.join();
    }
  }

  void startEnableRetryThread() {
    enable_retry_thread_running_ = true;
    controller_service_enable_retry_thread_ = std::thread([this]() {
      std::unique_lock<std::mutex> lock(enable_retry_mutex_);
      while (enable_retry_thread_running_) {
        for (auto it = controller_services_to_enable_.begin(); it != controller_services_to_enable_.end();) {
          if ((*it)->enable()) {
            it = controller_services_to_enable_.erase(it);
          } else {
            ++it;
          }
        }
        if (controller_services_to_enable_.empty()) {
          break;
        }
        enable_retry_condition_.wait_for(lock, admin_yield_duration_);
      }
      controller_services_to_enable_.clear();
    });
  }

  bool canEdit() override {
    return false;
  }

  ClassLoader &extension_loader_;

  std::shared_ptr<Configure> configuration_;

 private:
  std::chrono::milliseconds readAdministrativeYieldDuration() const {
    std::chrono::milliseconds admin_yield_duration = 30s;
    std::string yield_value_str;

    if (configuration_->get(Configure::nifi_administrative_yield_duration, yield_value_str)) {
      std::optional<core::TimePeriodValue> value = core::TimePeriodValue::fromString(yield_value_str);
      if (value) {
        admin_yield_duration = value->getMilliseconds();
      }
    }
    return admin_yield_duration;
  }

  std::thread controller_service_enable_retry_thread_;
  std::atomic_bool enable_retry_thread_running_{false};
  std::mutex enable_retry_mutex_;
  std::condition_variable enable_retry_condition_;
  std::vector<std::shared_ptr<ControllerServiceNode>> controller_services_to_enable_;
  std::chrono::milliseconds admin_yield_duration_;
  std::shared_ptr<logging::Logger> logger_;
};

}  // namespace org::apache::nifi::minifi::core::controller
