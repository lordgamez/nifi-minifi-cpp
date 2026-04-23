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

#include "utils/CallBackTimer.h"
#include <stdexcept>

namespace org::apache::nifi::minifi::utils {

CallBackTimer::CallBackTimer(std::chrono::milliseconds interval, const std::function<void(void)>& func) : execute_(false), func_(func), interval_(interval) {
}

CallBackTimer::~CallBackTimer() {
  logger_->log_debug("Stopping CallBackTimer in destructor");
  stop();
  // Do NOT hold mtx_ while joining: the timer callback may call stop() on this
  // timer (e.g. when all processors succeed), which also acquires mtx_. Joining
  // without the lock lets the callback complete and release any locks it holds.
  logger_->log_debug("Joining CallBackTimer thread in destructor");
  if (thd_.joinable()) {
    thd_.join();
  }
}

void CallBackTimer::stop() {
  logger_->log_debug("Stopping CallBackTimer");
  std::lock_guard<std::mutex> guard(mtx_);
  {
    logger_->log_debug("Acquiring lock to stop CallBackTimer");
    std::lock_guard<std::mutex> cv_guard(cv_mtx_);
    logger_->log_debug("Checking if CallBackTimer is already stopped");
    if (!execute_) {
      return;
    }
    execute_ = false;
    cv_.notify_all();
  }
}

void CallBackTimer::start() {
  logger_->log_debug("Starting CallBackTimer");
  std::lock_guard<std::mutex> guard(mtx_);
  {
    logger_->log_debug("Acquiring lock to start CallBackTimer");
    std::lock_guard<std::mutex> cv_guard(cv_mtx_);

    logger_->log_debug("Checking if CallBackTimer is already running");
    if (execute_) {
      return;
    }
  }

  logger_->log_debug("Joining CallBackTimer thread");
  if (thd_.joinable()) {
    thd_.join();
  }

  {
    logger_->log_debug("Acquiring lock to set CallBackTimer as running");
    std::lock_guard<std::mutex> cv_guard(cv_mtx_);
    logger_->log_debug("Setting CallBackTimer as running");
    execute_ = true;
  }

  logger_->log_debug("Starting CallBackTimer thread");
  thd_ = std::thread([this]() {
                       std::unique_lock<std::mutex> lk(cv_mtx_);
                       while (execute_) {
                         if (cv_.wait_for(lk, interval_, [this]{return !execute_;})) {
                           break;
                         }
                         lk.unlock();
                         this->func_();
                         lk.lock();
                       }
                     });
}

bool CallBackTimer::is_running() const {
  logger_->log_debug("Acquiring lock to check if CallBackTimer is running");
  std::lock_guard<std::mutex> guard(mtx_);
  logger_->log_debug("Checking if CallBackTimer is running");
  return execute_ && thd_.joinable();
}

}  // namespace org::apache::nifi::minifi::utils
