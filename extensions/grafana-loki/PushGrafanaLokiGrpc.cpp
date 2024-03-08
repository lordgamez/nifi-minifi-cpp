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
#include "PushGrafanaLokiGrpc.h"

#include <utility>

#include "utils/ProcessorConfigUtils.h"
#include "core/Resource.h"
#include "core/ProcessSession.h"
#include "google/protobuf/util/time_util.h"
#include "utils/file/FileUtils.h"

using namespace std::literals::chrono_literals;

namespace org::apache::nifi::minifi::extensions::grafana::loki {

void PushGrafanaLokiGrpc::initialize() {
  setSupportedProperties(Properties);
  setSupportedRelationships(Relationships);
}

void PushGrafanaLokiGrpc::setUpStreamLabels(core::ProcessContext& context) {
  auto stream_label_map = buildStreamLabelMap(context);
  std::stringstream formatted_labels;
  bool comma_needed = false;
  formatted_labels << "{";
  for (auto& [label_key, label_value] : stream_label_map) {
    if (comma_needed) {
      formatted_labels << ", ";
    }
    comma_needed = true;

    label_value = utils::string::replaceAll(label_value, "\"", "\\\"");
    formatted_labels << label_key << "=\"" << label_value << "\"";
  }
  formatted_labels << "}";
  stream_labels_ = formatted_labels.str();
}

void PushGrafanaLokiGrpc::setUpGrpcChannel(const std::string& url, core::ProcessContext& context) {
  if (auto keep_alive_time = context.getProperty<core::TimePeriodValue>(PushGrafanaLokiGrpc::KeepAliveTime)) {
    logger_->log_debug("PushGrafanaLokiGrpc Keep Alive Time is set to {} ms", keep_alive_time->getMilliseconds().count());
    args_.SetInt(GRPC_ARG_KEEPALIVE_TIME_MS, gsl::narrow<int>(keep_alive_time->getMilliseconds().count()));
  }

  if (auto keep_alive_timeout = context.getProperty<core::TimePeriodValue>(PushGrafanaLokiGrpc::KeepAliveTimeout)) {
    logger_->log_debug("PushGrafanaLokiGrpc Keep Alive Timeout is set to {} ms", keep_alive_timeout->getMilliseconds().count());
    args_.SetInt(GRPC_ARG_KEEPALIVE_TIMEOUT_MS, gsl::narrow<int>(keep_alive_timeout->getMilliseconds().count()));
  }

  if (auto max_pings_without_data = context.getProperty<uint64_t>(PushGrafanaLokiGrpc::MaxPingsWithoutData)) {
    logger_->log_debug("PushGrafanaLokiGrpc Max Pings Without Data is set to {}", *max_pings_without_data);
    args_.SetInt(GRPC_ARG_HTTP2_MAX_PINGS_WITHOUT_DATA, gsl::narrow<int>(*max_pings_without_data));
  }

  args_.SetInt(GRPC_ARG_KEEPALIVE_PERMIT_WITHOUT_CALLS, 1);
  if (auto ssl_context_service = getSSLContextService(context)) {
    ::grpc::SslCredentialsOptions ssl_credentials_options;
    ssl_credentials_options.pem_cert_chain = utils::file::FileUtils::get_content(ssl_context_service->getCertificateFile());
    ssl_credentials_options.pem_private_key = utils::file::FileUtils::get_content(ssl_context_service->getPrivateKeyFile());
    ssl_credentials_options.pem_root_certs = utils::file::FileUtils::get_content(ssl_context_service->getCACertificate());
    creds_ = ::grpc::SslCredentials(ssl_credentials_options);
  } else {
    creds_ = ::grpc::InsecureChannelCredentials();
  }
}

void PushGrafanaLokiGrpc::onSchedule(core::ProcessContext& context, core::ProcessSessionFactory& session_factory) {
  PushGrafanaLoki::onSchedule(context, session_factory);
  url_ = utils::getRequiredPropertyOrThrow<std::string>(context, Url.name);
  if (url_.empty()) {
    throw Exception(PROCESS_SCHEDULE_EXCEPTION, "Url property cannot be empty!");
  }
  tenant_id_ = context.getProperty(TenantID);
  if (auto connection_timeout = context.getProperty<core::TimePeriodValue>(PushGrafanaLokiGrpc::ConnectTimeout)) {
    connection_timeout_ms_ = connection_timeout->getMilliseconds();
  } else {
    throw Exception(PROCESS_SCHEDULE_EXCEPTION, "Invalid connection timeout is set.");
  }
  setUpGrpcChannel(url_, context);
}

nonstd::expected<void, std::string> PushGrafanaLokiGrpc::submitRequest(const std::vector<std::shared_ptr<core::FlowFile>>& batched_flow_files, core::ProcessSession& session) {
  // auto push_channel = ::grpc::CreateCustomChannel(url_, creds_, args_);
  using grpc::Channel;
  using grpc::ClientContext;
  using grpc::Status;
  using logproto::Pusher;
  using logproto::PushResponse;
  using logproto::PushRequest;

  std::unique_ptr<Pusher::Stub> stub_(Pusher::NewStub(grpc::CreateChannel(url_, grpc::InsecureChannelCredentials())));
  PushRequest request;

  std::string stream_labels_ = "id=a,job=minifi";
  logproto::StreamAdapter *stream = request.add_streams();
  if (stream == nullptr) {
    return nonstd::make_unexpected("failed to add stream");
  }
  stream->set_labels(stream_labels_);

  std::vector<std::string> flow_files {
    "test1", "test2", "test3"
  };

  std::map<std::string, std::string> log_line_metadata_attributes_ {
    {"asd", "val1"}, {"asd2", "val2"}
  };

  for (const auto& flow_file : flow_files) {
      logproto::EntryAdapter *entry = stream->add_entries();
      // auto timestamp_str = std::to_string(flow_file->getlineageStartDate().time_since_epoch() / std::chrono::nanoseconds(1));
      auto timestamp_nanos = 123456;
      *entry->mutable_timestamp() = google::protobuf::util::TimeUtil::NanosecondsToTimestamp(timestamp_nanos);

      entry->set_line(flow_file);

      for (const auto& [key, value] : log_line_metadata_attributes_) {
          logproto::LabelPairAdapter* label = entry->add_nonindexedlabels();
          label->set_name(key);
          label->set_value(value);
      }
  }

  // Container for the data we expect from the server.
  PushResponse reply;

  // Context for the client. It could be used to convey extra information to
  // the server and/or tweak certain RPC behaviors.
  ClientContext context;

  // The actual RPC.
  Status status = stub_->Push(&context, request, &reply);

  // Act upon its status.
  if (status.ok()) {
    return {}; // nonstd::make_unexpected("RPC OK");
  } else {
    std::cout << status.error_code() << ": " << status.error_message()
              << std::endl;
    return nonstd::make_unexpected("RPC failed");
  }
}

REGISTER_RESOURCE(PushGrafanaLokiGrpc, Processor);

}  // namespace org::apache::nifi::minifi::extensions::grafana::loki
