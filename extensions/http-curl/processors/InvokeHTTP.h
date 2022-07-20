/**
 * InvokeHTTP class declaration
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

#include <curl/curl.h>
#include <memory>
#include <string>
#include <map>

#include "FlowFileRecord.h"
#include "core/Processor.h"
#include "core/ProcessSession.h"
#include "core/Property.h"
#include "controllers/SSLContextService.h"
#include "core/logging/LoggerConfiguration.h"
#include "utils/Id.h"
#include "../client/HTTPClient.h"
#include "utils/Export.h"
#include "utils/Enum.h"
#include "utils/RegexUtils.h"

namespace org::apache::nifi::minifi::processors {

class InvokeHTTP : public core::Processor {
 public:
  SMART_ENUM(InvalidHTTPHeaderFieldHandlingOption,
    (FAIL, "fail"),
    (TRANSFORM, "transform"),
    (DROP, "drop")
  )

  explicit InvokeHTTP(const std::string& name, const utils::Identifier& uuid = {})
      : Processor(name, uuid) {
    setTriggerWhenEmpty(true);
  }

  EXTENSIONAPI static constexpr const char* Description = "An HTTP client processor which can interact with a configurable HTTP Endpoint. "
      "The destination URL and HTTP Method are configurable. FlowFile attributes are converted to HTTP headers and the "
      "FlowFile contents are included as the body of the request (if the HTTP Method is PUT, POST or PATCH).";

  EXTENSIONAPI static const core::Property Method;
  EXTENSIONAPI static const core::Property URL;
  EXTENSIONAPI static const core::Property ConnectTimeout;
  EXTENSIONAPI static const core::Property ReadTimeout;
  EXTENSIONAPI static const core::Property DateHeader;
  EXTENSIONAPI static const core::Property FollowRedirects;
  EXTENSIONAPI static const core::Property AttributesToSend;
  EXTENSIONAPI static const core::Property SSLContext;
  EXTENSIONAPI static const core::Property ProxyHost;
  EXTENSIONAPI static const core::Property ProxyPort;
  EXTENSIONAPI static const core::Property ProxyUsername;
  EXTENSIONAPI static const core::Property ProxyPassword;
  EXTENSIONAPI static const core::Property ContentType;
  EXTENSIONAPI static const core::Property SendBody;
  EXTENSIONAPI static const core::Property SendMessageBody;
  EXTENSIONAPI static const core::Property UseChunkedEncoding;
  EXTENSIONAPI static const core::Property DisablePeerVerification;
  EXTENSIONAPI static const core::Property PropPutOutputAttributes;
  EXTENSIONAPI static const core::Property AlwaysOutputResponse;
  EXTENSIONAPI static const core::Property PenalizeOnNoRetry;
  EXTENSIONAPI static const core::Property InvalidHTTPHeaderFieldHandlingStrategy;
  static auto properties() {
    return std::array{
      Method,
      URL,
      ConnectTimeout,
      ReadTimeout,
      DateHeader,
      FollowRedirects,
      AttributesToSend,
      SSLContext,
      ProxyHost,
      ProxyPort,
      ProxyUsername,
      ProxyPassword,
      ContentType,
      SendBody,
      SendMessageBody,
      UseChunkedEncoding,
      DisablePeerVerification,
      PropPutOutputAttributes,
      AlwaysOutputResponse,
      PenalizeOnNoRetry,
      InvalidHTTPHeaderFieldHandlingStrategy
    };
  }

  EXTENSIONAPI static const core::Relationship Success;
  EXTENSIONAPI static const core::Relationship RelResponse;
  EXTENSIONAPI static const core::Relationship RelRetry;
  EXTENSIONAPI static const core::Relationship RelNoRetry;
  EXTENSIONAPI static const core::Relationship RelFailure;
  static auto relationships() {
    return std::array{
      Success,
      RelResponse,
      RelRetry,
      RelNoRetry,
      RelFailure
    };
  }

  EXTENSIONAPI static constexpr bool SupportsDynamicProperties = false;
  EXTENSIONAPI static constexpr bool SupportsDynamicRelationships = false;
  EXTENSIONAPI static constexpr core::annotation::Input InputRequirement = core::annotation::Input::INPUT_ALLOWED;
  EXTENSIONAPI static constexpr bool IsSingleThreaded = false;

  ADD_COMMON_VIRTUAL_FUNCTIONS_FOR_PROCESSORS

  EXTENSIONAPI static std::string DefaultContentType;

  EXTENSIONAPI static constexpr const char* STATUS_CODE = "invokehttp.status.code";
  EXTENSIONAPI static constexpr const char* STATUS_MESSAGE = "invokehttp.status.message";
  EXTENSIONAPI static constexpr const char* RESPONSE_BODY = "invokehttp.response.body";
  EXTENSIONAPI static constexpr const char* REQUEST_URL = "invokehttp.request.url";
  EXTENSIONAPI static constexpr const char* TRANSACTION_ID = "invokehttp.tx.id";
  EXTENSIONAPI static constexpr const char* REMOTE_DN = "invokehttp.remote.dn";
  EXTENSIONAPI static constexpr const char* EXCEPTION_CLASS = "invokehttp.java.exception.class";
  EXTENSIONAPI static constexpr const char* EXCEPTION_MESSAGE = "invokehttp.java.exception.message";

  void onTrigger(core::ProcessContext *context, core::ProcessSession *session) override;
  void initialize() override;
  void onSchedule(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::ProcessSessionFactory> &sessionFactory) override;

 private:
  /**
   * Routes the flowfile to the proper destination
   * @param request request flow file record
   * @param response response flow file record
   * @param session process session
   * @param context process context
   * @param isSuccess success code or not
   * @param statuscode http response code.
   */
  void route(const std::shared_ptr<core::FlowFile> &request, const std::shared_ptr<core::FlowFile> &response, const std::shared_ptr<core::ProcessSession> &session,
             const std::shared_ptr<core::ProcessContext> &context, bool isSuccess, int64_t statusCode);
  bool shouldEmitFlowFile() const;
  [[nodiscard]] bool appendHeaders(const core::FlowFile& flow_file, /*std::invocable<std::string, std::string>*/ auto append_header);

  std::shared_ptr<minifi::controllers::SSLContextService> ssl_context_service_;
  std::string method_;
  std::string url_;
  bool date_header_include_{true};
  std::optional<utils::Regex> attributes_to_send_;
  std::chrono::milliseconds connect_timeout_ms_{20000};
  std::chrono::milliseconds read_timeout_ms_{20000};
  // attribute in which response body will be added
  std::string put_attribute_name_;
  bool always_output_response_{false};
  std::string content_type_;
  bool use_chunked_encoding_{false};
  bool penalize_no_retry_{false};
  // disabling peer verification makes susceptible for MITM attacks
  bool disable_peer_verification_{false};
  utils::HTTPProxy proxy_;
  bool follow_redirects_{true};
  bool send_body_{true};
  InvalidHTTPHeaderFieldHandlingOption invalid_http_header_field_handling_strategy_;
  std::shared_ptr<core::logging::Logger> logger_{core::logging::LoggerFactory<InvokeHTTP>::getLogger()};
};

}  // namespace org::apache::nifi::minifi::processors
