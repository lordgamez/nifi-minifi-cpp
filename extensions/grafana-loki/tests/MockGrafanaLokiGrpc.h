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

#include <thread>
#include <memory>
#include <string>
#include <vector>
#include <thread>
#include "grpcpp/grpcpp.h"
#include "grafana-loki-push.grpc.pb.h"
#include "google/protobuf/util/time_util.h"
#include "core/logging/Logger.h"
#include "core/logging/LoggerConfiguration.h"

using namespace std::literals::chrono_literals;

namespace org::apache::nifi::minifi::extensions::grafana::loki::test {

struct GrafanaLokiLineEntry {
  uint64_t timestamp = 0;
  std::string line;
  std::map<std::string, std::string> labels;
};

struct GrafanaLokiGrpcRequest {
  void reset() {
    stream_labels.clear();
    entries.clear();
  }

  std::string stream_labels;
  std::vector<GrafanaLokiLineEntry> entries;
};

class GrafanaLokiGrpcService final : public ::logproto::Pusher::Service {
 public:
  ::grpc::Status Push(::grpc::ServerContext* ctx, const ::logproto::PushRequest* request, ::logproto::PushResponse* response) override {
    // tenant_id_.clear();
    // auto metadata = ctx->client_metadata();
    // auto org_id = metadata.find("x-scope-orgid");
    // if (org_id != metadata.end()) {
    //   tenant_id_ = std::string(org_id->second.data(), org_id->second.size());
    // }
    // last_request_received_.reset();
    // auto& stream = request->streams(0);
    // last_request_received_.stream_labels = stream.labels();
    // for (int i = 0; i < stream.entries_size(); ++i) {
    //   auto& request_entry = stream.entries(i);
    //   GrafanaLokiLineEntry entry;
    //   entry.timestamp = google::protobuf::util::TimeUtil::TimestampToNanoseconds(request_entry.timestamp());;
    //   entry.line = request_entry.line();
    //   for (int j = 0; j < request_entry.nonindexedlabels_size(); ++j) {
    //     auto& label = request_entry.nonindexedlabels(j);
    //     entry.labels[label.name()] = label.value();
    //   }
    //   last_request_received_.entries.push_back(entry);
    // }
    return ::grpc::Status::OK;
  }

  GrafanaLokiGrpcRequest getLastRequest() const {
    return last_request_received_;
  }

  std::string getLastTenantId() const {
    return tenant_id_;
  }

 private:
  GrafanaLokiGrpcRequest last_request_received_;
  std::string tenant_id_;
};

class MockGrafanaLokiGrpc {
 public:
  explicit MockGrafanaLokiGrpc(std::string port) {
    logger_->log_error("Starting gRPC server on port {}", port);
    // server_address_ = "0.0.0.0:" + port;
    // server_builder_.AddListeningPort(server_address_, ::grpc::InsecureServerCredentials());
    // server_builder_.RegisterService(&loki_grpc_service_);
    // grpc_server_ = server_builder_.BuildAndStart();
  }

  GrafanaLokiGrpcRequest getLastRequest() const {
    return loki_grpc_service_.getLastRequest();
  }

  std::string getLastTenantId() const {
    return loki_grpc_service_.getLastTenantId();
  }

  void RunServer(uint16_t port) {
    std::string server_address = "0.0.0.0:50051";
    GrafanaLokiGrpcService service;

    // grpc::EnableDefaultHealthCheckService(true);
    // grpc::reflection::InitProtoReflectionServerBuilderPlugin();
    ::grpc::ServerBuilder builder;
    // Listen on the given address without any authentication mechanism.
    builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
    // Register "service" as the instance through which we'll communicate with
    // clients. In this case it corresponds to an *synchronous* service.
    builder.RegisterService(&service);
    // Finally assemble the server.
    std::unique_ptr<::grpc::Server> server(builder.BuildAndStart());

    // Wait for the server to shutdown. Note that some other thread must be
    // responsible for shutting down the server for this call to ever return.
    grpc_server_->Wait();
  }

  void stop() {
    grpc_server_->Shutdown();
  }

  ~MockGrafanaLokiGrpc() {
    logger_->log_error("SHUTTING DOWN");
    grpc_server_->Shutdown();
  }

 private:
  std::shared_ptr<org::apache::nifi::minifi::core::logging::Logger> logger_ = org::apache::nifi::minifi::core::logging::LoggerFactory<MockGrafanaLokiGrpc>::getLogger();
  GrafanaLokiGrpcService loki_grpc_service_;
  std::unique_ptr<::grpc::Server> grpc_server_;
  ::grpc::ServerBuilder server_builder_;
  std::string server_address_;
};

}  // namespace org::apache::nifi::minifi::extensions::grafana::loki::test
