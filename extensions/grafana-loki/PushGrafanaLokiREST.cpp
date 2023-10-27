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


#include "PushGrafanaLokiREST.h"

#include <utility>

#include "core/ProcessContext.h"
#include "core/ProcessSession.h"
#include "core/Resource.h"
#include "utils/ProcessorConfigUtils.h"
#include "utils/StringUtils.h"

namespace org::apache::nifi::minifi::extensions::grafana::loki {

void PushGrafanaLokiREST::LogBatch::add(const std::shared_ptr<core::FlowFile>& flowfile) {
  gsl_Expects(state_manager_);
  if (max_batch_wait_ && batched_flowfiles_.empty()) {
    start_push_time_ = std::chrono::steady_clock::now();
    std::unordered_map<std::string, std::string> state;
    state["start_push_time"] = std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(start_push_time_.time_since_epoch()).count());
    state_manager_->set(state);
  }
  batched_flowfiles_.push_back(flowfile);
}

std::vector<std::shared_ptr<core::FlowFile>> PushGrafanaLokiREST::LogBatch::flush() {
  gsl_Expects(state_manager_);
  start_push_time_ = {};
  auto result = std::move(batched_flowfiles_);
  if (max_batch_wait_) {
    start_push_time_ = {};
    std::unordered_map<std::string, std::string> state;
    state["start_push_time"] = "0";
    state_manager_->set(state);
  }
  return result;
}

bool PushGrafanaLokiREST::LogBatch::isReady() const {
  return (max_batch_size_ &&  batched_flowfiles_.size() >= *max_batch_size_) || (max_batch_wait_ && std::chrono::steady_clock::now() - start_push_time_ >= *max_batch_wait_);
}

void PushGrafanaLokiREST::LogBatch::setMaxBatchSize(std::optional<uint64_t> max_batch_size) {
  max_batch_size_ = max_batch_size;
}

void PushGrafanaLokiREST::LogBatch::setMaxBatchWait(std::optional<std::chrono::milliseconds> max_batch_wait) {
  max_batch_wait_ = max_batch_wait;
}

void PushGrafanaLokiREST::LogBatch::setStateManager(core::StateManager* state_manager) {
  state_manager_ = state_manager;
}

const core::Relationship PushGrafanaLokiREST::Self("__self__", "Marks the FlowFile to be owned by this processor");

void PushGrafanaLokiREST::initialize() {
  setSupportedProperties(Properties);
  setSupportedRelationships(Relationships);
}

namespace {
auto getSSLContextService(core::ProcessContext& context) {
  if (auto ssl_context = context.getProperty(PushGrafanaLokiREST::SSLContextService)) {
    return std::dynamic_pointer_cast<minifi::controllers::SSLContextService>(context.getControllerService(*ssl_context));
  }
  return std::shared_ptr<minifi::controllers::SSLContextService>{};
}

void setupClientTimeouts(extensions::curl::HTTPClient& client, const core::ProcessContext& context) {
  if (auto connection_timeout = context.getProperty<core::TimePeriodValue>(PushGrafanaLokiREST::ConnectTimeout)) {
    client.setConnectionTimeout(connection_timeout->getMilliseconds());
  }

  if (auto read_timeout = context.getProperty<core::TimePeriodValue>(PushGrafanaLokiREST::ReadTimeout)) {
    client.setReadTimeout(read_timeout->getMilliseconds());
  }
}
} // namespace

void PushGrafanaLokiREST::onSchedule(const std::shared_ptr<core::ProcessContext>& context, const std::shared_ptr<core::ProcessSessionFactory>&) {
  gsl_Expects(context);
  auto state_manager = context->getStateManager();
  if (state_manager == nullptr) {
    throw Exception(PROCESSOR_EXCEPTION, "Failed to get StateManager");
  }
  log_batch_.setStateManager(state_manager);

  std::unordered_map<std::string, std::string> state_map;
  if (state_manager->get(state_map)) {
    auto it = state_map.find("start_push_time");
    if (it != state_map.end()) {
      std::chrono::steady_clock::time_point start_push_time{std::chrono::milliseconds{std::stoll(it->second)}};
      log_batch_.setStartPushTime(start_push_time);
    }
  }

  auto url = utils::getRequiredPropertyOrThrow<std::string>(*context, URL.name);
  client_.initialize(utils::HttpRequestMethod::POST, url, getSSLContextService(*context));
  client_.setContentType("application/json");

  if (auto stream_label_attributes = context->getProperty(StreamLabelAttributes)) {
    stream_label_attributes_ = utils::StringUtils::split(*stream_label_attributes, ",");
  } else {
    throw Exception(PROCESS_SCHEDULE_EXCEPTION, "Missing or invalid Stream Label Attributes");
  }

  if (auto log_line_label_attributes = context->getProperty(LogLineLabelAttributes)) {
    log_line_label_attributes_ = utils::StringUtils::split(*log_line_label_attributes, ",");
  }

  tenant_id_ = context->getProperty(TenantID);
  auto max_batch_wait = context->getProperty<core::TimePeriodValue>(BatchWait);

  auto max_batch_size = context->getProperty<uint64_t>(BatchSize);
  if (!max_batch_size && !max_batch_wait) {
    throw Exception(PROCESS_SCHEDULE_EXCEPTION, "Batch Size or Batch Wait property must be set!");
  }

  if (max_batch_size && *max_batch_size < 1) {
    throw Exception(PROCESS_SCHEDULE_EXCEPTION, "Batch Size property is invalid!");
  }

  log_batch_.setMaxBatchSize(max_batch_size);
  if (max_batch_wait) {
    log_batch_.setMaxBatchWait(max_batch_wait->getMilliseconds());
  }

  setupClientTimeouts(client_, *context);
  bool use_chunked_encoding = (context->getProperty(UseChunkedEncoding) | utils::andThen(&utils::StringUtils::toBool)).value_or(false);
  if (use_chunked_encoding) {
    client_.setRequestHeader("Transfer-Encoding", "chunked");
  } else {
    client_.setRequestHeader("Transfer-Encoding", std::nullopt);
  }
}

void PushGrafanaLokiREST::onTrigger(const std::shared_ptr<core::ProcessContext>& context, const std::shared_ptr<core::ProcessSession>& session) {
  gsl_Expects(context && session);

}

void PushGrafanaLokiREST::restore(const std::shared_ptr<core::FlowFile>& flow_file) {
  if (!flow_file) {
    return;
  }
  log_batch_.add(flow_file);
}

REGISTER_RESOURCE(PushGrafanaLokiREST, Processor);

}  // namespace org::apache::nifi::minifi::extensions::grafana::loki
