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

#include <memory>
#include <string>

#include "opc.h"
#include "opcbase.h"
#include "FlowFileRecord.h"
#include "core/Processor.h"
#include "core/ProcessSession.h"
#include "core/Core.h"

namespace org::apache::nifi::minifi::processors {

  void BaseOPCProcessor::onSchedule(core::ProcessContext& context, core::ProcessSessionFactory&) {
    logger_->log_trace("BaseOPCProcessor::onSchedule");

    application_uri_.clear();
    cert_buffer_.clear();
    key_buffer_.clear();
    password_.clear();
    username_.clear();
    trust_buffers_.clear();

    context.getProperty(OPCServerEndPoint, endpoint_url);
    context.getProperty(ApplicationURI, application_uri_);

    if (context.getProperty(Username, username_) != context.getProperty(Password, password_)) {
      throw Exception(PROCESS_SCHEDULE_EXCEPTION, "Both or neither of Username and Password should be provided!");
    }

    auto certificate_path_res = context.getProperty(CertificatePath, certpath_);
    auto key_path_res = context.getProperty(KeyPath, keypath_);
    context.getProperty(TrustedPath, trustpath_);
    if (certificate_path_res != key_path_res) {
      throw Exception(PROCESS_SCHEDULE_EXCEPTION, "All or none of Certificate path and Key path should be provided!");
    }

    if (certpath_.empty()) {
      return;
    }
    if (application_uri_.empty()) {
      throw Exception(PROCESS_SCHEDULE_EXCEPTION, "Application URI must be provided if Certificate path is provided!");
    }

    std::ifstream input_cert(certpath_, std::ios::binary);
    if (input_cert.good()) {
      cert_buffer_ = std::vector<char>(std::istreambuf_iterator<char>(input_cert), {});
    }
    std::ifstream input_key(keypath_, std::ios::binary);
    if (input_key.good()) {
      key_buffer_ = std::vector<char>(std::istreambuf_iterator<char>(input_key), {});
    }

    if (cert_buffer_.empty()) {
      auto error_msg = utils::string::join_pack("Failed to load cert from path: ", certpath_);
      throw Exception(PROCESS_SCHEDULE_EXCEPTION, error_msg);
    }
    if (key_buffer_.empty()) {
      auto error_msg = utils::string::join_pack("Failed to load key from path: ", keypath_);
      throw Exception(PROCESS_SCHEDULE_EXCEPTION, error_msg);
    }

    if (!trustpath_.empty()) {
      std::ifstream input_trust(trustpath_, std::ios::binary);
      if (input_trust.good()) {
        trust_buffers_.push_back(std::vector<char>(std::istreambuf_iterator<char>(input_trust), {}));
      } else {
        auto error_msg = utils::string::join_pack("Failed to load trusted server certs from path: ", trustpath_);
        throw Exception(PROCESS_SCHEDULE_EXCEPTION, error_msg);
      }
    }
  }

  bool BaseOPCProcessor::reconnect() {
    if (connection_ == nullptr) {
      connection_ = opc::Client::createClient(logger_, application_uri_, cert_buffer_, key_buffer_, trust_buffers_);
    }

    if (connection_->isConnected()) {
      return true;
    }

    auto sc = connection_->connect(endpoint_url, username_, password_);
    if (sc != UA_STATUSCODE_GOOD) {
      logger_->log_error("Failed to connect: {}!", UA_StatusCode_name(sc));
      return false;
    }
    logger_->log_debug("Successfully connected.");
    return true;
  }

  void BaseOPCProcessor::readPathReferenceTypes(core::ProcessContext& context, const std::string& node_id) {
    std::string value;
    context.getProperty(PathReferenceTypes, value);
    if (value.empty()) {
      return;
    }
    auto pathReferenceTypes = utils::string::splitAndTrimRemovingEmpty(value, "/");
    if (pathReferenceTypes.size() != utils::string::splitAndTrimRemovingEmpty(node_id, "/").size() - 1) {
      throw Exception(PROCESS_SCHEDULE_EXCEPTION, "Path reference types must be provided for each node pair in the path!");
    }
    for (const auto& pathReferenceType : pathReferenceTypes) {
      if (pathReferenceType == "Organizes") {
        path_reference_types_.push_back(UA_NS0ID_ORGANIZES);
      } else if (pathReferenceType == "HasComponent") {
        path_reference_types_.push_back(UA_NS0ID_HASCOMPONENT);
      } else if (pathReferenceType == "HasProperty") {
        path_reference_types_.push_back(UA_NS0ID_HASPROPERTY);
      } else {
        throw Exception(PROCESS_SCHEDULE_EXCEPTION, fmt::format("Unsupported reference type set in 'Path reference types' property: '{}'.", pathReferenceType));
      }
    }
  }

}  // namespace org::apache::nifi::minifi::processors
