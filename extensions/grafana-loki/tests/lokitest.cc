/*
 *
 * Copyright 2015 gRPC authors.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 */
#include "Catch.h"

#define NOMINMAX
#ifdef WIN32
#pragma push_macro("GetObject")
#undef GetObject  // windows.h #defines GetObject = GetObjectA or GetObjectW, which conflicts with rapidjson
#endif

#include <limits>
#include <iostream>
#include <memory>
#include <string>
#include <vector>
#include <thread>

// #include "absl/flags/flag.h"
// #include "absl/flags/parse.h"
#include "google/protobuf/util/time_util.h"

#include <grpcpp/grpcpp.h>

#ifdef BAZEL_BUILD
#include "examples/protos/push.grpc.pb.h"
#else
// #include "push.grpc.pb.h"
#include "grafana-loki-push.grpc.pb.h"
#endif

// ABSL_FLAG(std::string, target, "localhost:50051", "Server address");

using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::Channel;
using grpc::ClientContext;
using grpc::Status;
using logproto::Pusher;
using logproto::PushResponse;
using logproto::PushRequest;

class PusherClient {
 public:
  PusherClient(std::shared_ptr<Channel> channel)
      : stub_(Pusher::NewStub(channel)) {}

  // Assembles the client's payload, sends it and presents the response back
  // from the server.
  std::string Push(const std::string& user) {
    // Data we are sending to the server.
    PushRequest request;

    std::string stream_labels_ = "id=a,job=minifi";
    logproto::StreamAdapter *stream = request.add_streams();
    if (stream == nullptr) {
      return "failed to add stream";
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
      return "RPC OK";
    } else {
      std::cout << status.error_code() << ": " << status.error_message()
                << std::endl;
      return "RPC failed";
    }
  }

 private:
  std::unique_ptr<Pusher::Stub> stub_;
};

class PusherServiceImpl final : public Pusher::Service {
  ::grpc::Status Push(::grpc::ServerContext* ctx, const ::logproto::PushRequest* request, ::logproto::PushResponse* response) override {
    return Status::OK;
  }
};

void RunServer(uint16_t port) {
  std::string server_address = "0.0.0.0:50051";
  PusherServiceImpl service;

  // grpc::EnableDefaultHealthCheckService(true);
  // grpc::reflection::InitProtoReflectionServerBuilderPlugin();
  ServerBuilder builder;
  // Listen on the given address without any authentication mechanism.
  builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
  // Register "service" as the instance through which we'll communicate with
  // clients. In this case it corresponds to an *synchronous* service.
  builder.RegisterService(&service);
  // Finally assemble the server.
  std::unique_ptr<Server> server(builder.BuildAndStart());
  std::cout << "Server listening on " << server_address << std::endl;

  // Wait for the server to shutdown. Note that some other thread must be
  // responsible for shutting down the server for this call to ever return.
  server->Wait();
}

TEST_CASE("Url property is required", "[PushGrafanaLokiGrpc]") {
  std::thread([]() { RunServer(50051); }).detach();
  std::this_thread::sleep_for(std::chrono::seconds(1));

  // absl::ParseCommandLine(argc, argv);
  // Instantiate the client. It requires a channel, out of which the actual RPCs
  // are created. This channel models a connection to an endpoint specified by
  // the argument "--target=" which is the only expected argument.
  // std::string target_str = absl::GetFlag(FLAGS_target);
  // We indicate that the channel isn't authenticated (use of
  // InsecureChannelCredentials()).
  PusherClient pusher(
      grpc::CreateChannel("localhost:50051", grpc::InsecureChannelCredentials()));
  std::string user("world");
  std::string reply = pusher.Push(user);
}

#ifdef WIN32
#pragma pop_macro("GetObject")
#endif
