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
#include "rapidjson/document.h"
#include "rapidjson/stream.h"
#include "rapidjson/writer.h"

namespace org::apache::nifi::minifi::extensions::grafana::loki {

void PushGrafanaLokiREST::LogBatch::add(const std::shared_ptr<core::FlowFile>& flowfile) {
  gsl_Expects(state_manager_);
  if (log_line_batch_wait_ && batched_flowfiles_.empty()) {
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
  if (log_line_batch_wait_) {
    start_push_time_ = {};
    std::unordered_map<std::string, std::string> state;
    state["start_push_time"] = "0";
    state_manager_->set(state);
  }
  return result;
}

bool PushGrafanaLokiREST::LogBatch::isReady() const {
  return (log_line_batch_size_ && batched_flowfiles_.size() >= *log_line_batch_size_) || (log_line_batch_wait_ && std::chrono::steady_clock::now() - start_push_time_ >= *log_line_batch_wait_);
}

void PushGrafanaLokiREST::LogBatch::setLogLineBatchSize(std::optional<uint64_t> log_line_batch_size) {
  log_line_batch_size_ = log_line_batch_size;
}

void PushGrafanaLokiREST::LogBatch::setLogLineBatchWait(std::optional<std::chrono::milliseconds> log_line_batch_wait) {
  log_line_batch_wait_ = log_line_batch_wait;
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

  if (auto stream_labels_str = context->getProperty(StreamLabels)) {
    auto stream_labels = utils::StringUtils::splitAndTrimRemovingEmpty(*stream_labels_str, ",");
    if (stream_labels.empty()) {
      throw Exception(PROCESS_SCHEDULE_EXCEPTION, "Missing or invalid Stream Label Attributes");
    }
    for (const auto& label : stream_labels) {
      auto stream_labels = utils::StringUtils::splitAndTrimRemovingEmpty(label, "=");
      if (stream_labels.size() != 2) {
        throw Exception(PROCESS_SCHEDULE_EXCEPTION, "Missing or invalid Stream Label Attributes");
      }
      stream_label_attributes_[stream_labels[0]] = stream_labels[1];
    }
  } else {
    throw Exception(PROCESS_SCHEDULE_EXCEPTION, "Missing or invalid Stream Label Attributes");
  }

  if (auto log_line_label_attributes = context->getProperty(LogLineLabelAttributes)) {
    log_line_label_attributes_ = utils::StringUtils::splitAndTrimRemovingEmpty(*log_line_label_attributes, ",");
  }

  tenant_id_ = context->getProperty(TenantID);
  auto log_line_batch_wait = context->getProperty<core::TimePeriodValue>(LogLineBatchWait);

  auto log_line_batch_size = context->getProperty<uint64_t>(LogLineBatchSize);
  if (!log_line_batch_size && !log_line_batch_wait) {
    throw Exception(PROCESS_SCHEDULE_EXCEPTION, "Batch Size or Batch Wait property must be set!");
  }

  if (log_line_batch_size && *log_line_batch_size < 1) {
    throw Exception(PROCESS_SCHEDULE_EXCEPTION, "Batch Size property is invalid!");
  }

  max_batch_size_ = context->getProperty<uint64_t>(MaxBatchSize);

  log_batch_.setLogLineBatchSize(log_line_batch_size);
  if (log_line_batch_wait) {
    log_batch_.setLogLineBatchWait(log_line_batch_wait->getMilliseconds());
  }

  setupClientTimeouts(client_, *context);
  bool use_chunked_encoding = (context->getProperty(UseChunkedEncoding) | utils::andThen(&utils::StringUtils::toBool)).value_or(false);
  if (use_chunked_encoding) {
    client_.setRequestHeader("Transfer-Encoding", "chunked");
  } else {
    client_.setRequestHeader("Transfer-Encoding", std::nullopt);
  }
}

std::string PushGrafanaLokiREST::createLokiJson(const std::vector<std::shared_ptr<core::FlowFile>>& batched_flow_files, core::ProcessSession& session) const {
  rapidjson::Document document;
  document.SetObject();
  rapidjson::Document::AllocatorType& allocator = document.GetAllocator();

  rapidjson::Value streams(rapidjson::kArrayType);
  rapidjson::Value streamObject(rapidjson::kObjectType);

  for (const auto& [key, value] : stream_label_attributes_) {
    rapidjson::Value label(value.c_str(), allocator);
    streamObject.AddMember(label, rapidjson::Value(key.c_str(), allocator), allocator);
  }

  rapidjson::Value values(rapidjson::kArrayType);
  std::string line;
  for (const auto& flow_file : batched_flow_files) {
    session.read(flow_file, [this, &flow_file, &line](const std::shared_ptr<io::InputStream>& input_stream) -> int64_t {
      const size_t BUFFER_SIZE = 8192;
      std::array<char, BUFFER_SIZE> buffer;
      size_t read_size = 0;
      while (read_size < input_stream->size()) {
        const size_t next_read_size = (std::min)(gsl::narrow<size_t>(input_stream->size() - read_size), BUFFER_SIZE);
        const auto ret = input_stream->read(as_writable_bytes(std::span(buffer).subspan(0, next_read_size)));
        if (io::isError(ret)) {
          return -1;
        } else if (ret == 0) {
          break;
        } else {
          line.append(buffer.data(), ret);
          read_size += ret;
        }
      }
      return gsl::narrow<int64_t>(read_size);
    });
    rapidjson::Value log_line(rapidjson::kArrayType);

    auto timestamp_str = std::to_string(flow_file->getlineageStartDate().time_since_epoch() / std::chrono::nanoseconds(1));
    rapidjson::Value timestamp;
    timestamp.SetString(timestamp_str.c_str(), gsl::narrow<rapidjson::SizeType>(timestamp_str.length()));
    rapidjson::Value log_line_value;
    log_line_value.SetString(line.c_str(), gsl::narrow<rapidjson::SizeType>(line.length()));

    log_line.PushBack(timestamp, allocator);
    log_line.PushBack(log_line_value, allocator);
    values.PushBack(log_line, allocator);
  }

  streamObject.AddMember("stream", streamObject, allocator);
  streamObject.AddMember("values", values, allocator);
  streams.PushBack(streamObject, allocator);
  document.AddMember("streams", streams, allocator);
  rapidjson::StringBuffer buffer;
  rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
  document.Accept(writer);
  return buffer.GetString();
}

nonstd::expected<void, std::string> PushGrafanaLokiREST::submitRequest(const std::string& loki_json) {
  client_.setPostFields(loki_json);
  if (!client_.submit()) {
    return nonstd::make_unexpected("Submit failed");
  }
  auto response_code = client_.getResponseCode();
  if (response_code < 200 || response_code >= 300) {
    return nonstd::make_unexpected("Error occurred: " + std::to_string(response_code) + ", " + client_.getResponseBody().data());
  }
  return {};
}

void PushGrafanaLokiREST::processBatch(const std::vector<std::shared_ptr<core::FlowFile>>& batched_flow_files, core::ProcessSession& session) {
  if (batched_flow_files.empty()) {
    return;
  }

  auto loki_json = createLokiJson(batched_flow_files, session);
  auto result = submitRequest(loki_json);
  if (!result) {
    logger_->log_error("Failed to send log batch to Loki: {}", result.error());
    for (const auto& flow_file : batched_flow_files) {
      session.transfer(flow_file, Failure);
    }
  } else {
    for (const auto& flow_file : batched_flow_files) {
      session.transfer(flow_file, Success);
    }
  }
}

void PushGrafanaLokiREST::onTrigger(const std::shared_ptr<core::ProcessContext>& context, const std::shared_ptr<core::ProcessSession>& session) {
  gsl_Expects(context && session);
  uint64_t flow_files_read = 0;
  while (max_batch_size_ || *max_batch_size_ == 0 || flow_files_read < *max_batch_size_) {
    std::shared_ptr<core::FlowFile> flow_file = session->get();
    if (!flow_file) {
      return;
    }

    log_batch_.add(flow_file);
    if (log_batch_.isReady()) {
      auto batched_flow_files = log_batch_.flush();
      processBatch(batched_flow_files, *session);
    }

    ++flow_files_read;
  }
}

void PushGrafanaLokiREST::restore(const std::shared_ptr<core::FlowFile>& flow_file) {
  if (!flow_file) {
    return;
  }
  log_batch_.add(flow_file);
}

REGISTER_RESOURCE(PushGrafanaLokiREST, Processor);

}  // namespace org::apache::nifi::minifi::extensions::grafana::loki
