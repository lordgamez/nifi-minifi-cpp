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

#include <string>
#include <memory>
#include <vector>
#include <map>

#include "controllers/SSLContextService.h"
#include "core/Processor.h"
#include "core/PropertyDefinition.h"
#include "core/PropertyDefinitionBuilder.h"
#include "core/PropertyType.h"
#include "core/RelationshipDefinition.h"
#include "client/HTTPClient.h"
#include "core/StateManager.h"

namespace org::apache::nifi::minifi::extensions::grafana::loki {

class PushGrafanaLokiREST : public core::Processor {
 public:
  EXTENSIONAPI static constexpr const char* Description = "A Grafana Loki push processor that uses the Grafana Loki REST API.";

  explicit PushGrafanaLokiREST(const std::string& name, const utils::Identifier& uuid = {})
      : Processor(name, uuid) {
  }
  ~PushGrafanaLokiREST() override = default;

  EXTENSIONAPI static constexpr auto Url = core::PropertyDefinitionBuilder<>::createProperty("Url")
    .withDescription("Url of loki server API endpoint.")
    .isRequired(true)
    .build();
  EXTENSIONAPI static constexpr auto StreamLabels = core::PropertyDefinitionBuilder<>::createProperty("Stream Labels")
    .withDescription("Comma separated list of <key>=<value> labels to be sent as stream labels.")
    .isRequired(true)
    .build();
  EXTENSIONAPI static constexpr auto LogLineLabelAttributes = core::PropertyDefinitionBuilder<>::createProperty("Log Line Label Attributes")
    .withDescription("Comma separated list of attributes to be sent as log line labels.")
    .build();
  EXTENSIONAPI static constexpr auto TenantID = core::PropertyDefinitionBuilder<>::createProperty("Tenant ID")
    .withDescription("The tenant ID used by default to push logs to Grafana Loki. If omitted or empty it assumes Grafana Loki is running in single-tenant mode and no X-Scope-OrgID header is sent.")
    .build();
  EXTENSIONAPI static constexpr auto MaxBatchSize = core::PropertyDefinitionBuilder<>::createProperty("Max Batch Size")
    .withDescription("The maximum number of flow files to process at a time.")
    .withPropertyType(core::StandardPropertyTypes::UNSIGNED_LONG_TYPE)
    .withDefaultValue("100")
    .build();
  EXTENSIONAPI static constexpr auto LogLineBatchWait = core::PropertyDefinitionBuilder<>::createProperty("Log Line Batch Wait")
    .withDescription("Time to wait before sending a log line batch to Grafana Loki, full or not.")
    .withPropertyType(core::StandardPropertyTypes::TIME_PERIOD_TYPE)
    .build();
  EXTENSIONAPI static constexpr auto LogLineBatchSize = core::PropertyDefinitionBuilder<>::createProperty("Log Line Batch Size")
    .withDescription("Number of log lines to send in a batch to Loki")
    .withPropertyType(core::StandardPropertyTypes::UNSIGNED_INT_TYPE)
    .build();
  EXTENSIONAPI static constexpr auto ConnectTimeout = core::PropertyDefinitionBuilder<>::createProperty("Connection Timeout")
    .withDescription("Max wait time for connection to the Grafana Loki service")
    .withPropertyType(core::StandardPropertyTypes::TIME_PERIOD_TYPE)
    .withDefaultValue("5 s")
    .isRequired(true)
    .build();
  EXTENSIONAPI static constexpr auto ReadTimeout = core::PropertyDefinitionBuilder<>::createProperty("Read Timeout")
    .withDescription("Max wait time for response from remote service")
    .withPropertyType(core::StandardPropertyTypes::TIME_PERIOD_TYPE)
    .withDefaultValue("15 s")
    .isRequired(true)
    .build();
  EXTENSIONAPI static constexpr auto UseChunkedEncoding = core::PropertyDefinitionBuilder<>::createProperty("Use Chunked Encoding")
    .withDescription("Set this property to true in order to not pass the 'Content-length' header and instead send 'Transfer-Encoding' with a value of 'chunked'. "
                     "This will enable the data transfer mechanism which was introduced in HTTP 1.1 to pass data of unknown lengths in chunks.")
    .withPropertyType(core::StandardPropertyTypes::BOOLEAN_TYPE)
    .withDefaultValue("false")
    .isRequired(true)
    .build();
  EXTENSIONAPI static constexpr auto SSLContextService = core::PropertyDefinitionBuilder<>::createProperty("SSL Context Service")
    .withDescription("The SSL Context Service used to provide client certificate information for TLS/SSL (https) connections.")
    .withAllowedTypes<minifi::controllers::SSLContextService>()
    .build();
  EXTENSIONAPI static constexpr auto Properties = std::array<core::PropertyReference, 12>{
      Url,
      StreamLabels,
      LogLineLabelAttributes,
      TenantID,
      MaxBatchSize,
      LogLineBatchWait,
      LogLineBatchSize,
      ConnectTimeout,
      ReadTimeout,
      UseChunkedEncoding,
      SSLContextService
  };

  EXTENSIONAPI static constexpr auto Success = core::RelationshipDefinition{"success", "All flowfiles that succeed in being transferred into Grafana Loki go here."};
  EXTENSIONAPI static constexpr auto Failure = core::RelationshipDefinition{"failure", "All flowfiles that fail for reasons unrelated to server availability go to this relationship."};
  EXTENSIONAPI static constexpr auto Relationships = std::array{Success, Failure};

  EXTENSIONAPI static constexpr bool SupportsDynamicProperties = false;
  EXTENSIONAPI static constexpr bool SupportsDynamicRelationships = false;
  EXTENSIONAPI static constexpr core::annotation::Input InputRequirement = core::annotation::Input::INPUT_REQUIRED;
  EXTENSIONAPI static constexpr bool IsSingleThreaded = true;

  ADD_COMMON_VIRTUAL_FUNCTIONS_FOR_PROCESSORS

  void initialize() override;
  void onSchedule(const std::shared_ptr<core::ProcessContext>& context, const std::shared_ptr<core::ProcessSessionFactory>& sessionFactory) override;
  void onTrigger(const std::shared_ptr<core::ProcessContext>& context, const std::shared_ptr<core::ProcessSession>& session) override;
  void restore(const std::shared_ptr<core::FlowFile>& flow_file) override;
  std::set<core::Connectable*> getOutGoingConnections(const std::string &relationship) override;

 protected:
  static const core::Relationship Self;

 private:
  class LogBatch {
   public:
    void add(const std::shared_ptr<core::FlowFile>& flowfile);
    bool isReady() const;
    std::vector<std::shared_ptr<core::FlowFile>> flush();
    void setLogLineBatchSize(std::optional<uint64_t> log_line_batch_size);
    void setLogLineBatchWait(std::optional<std::chrono::milliseconds> log_line_batch_wait);
    void setStateManager(core::StateManager* state_manager);
    void setStartPushTime(std::chrono::steady_clock::time_point start_push_time);

   private:
    std::optional<uint64_t> log_line_batch_size_ = 1;
    std::optional<std::chrono::milliseconds> log_line_batch_wait_;
    std::chrono::steady_clock::time_point start_push_time_;
    uint64_t batched_flowfiles_size_ = 0;
    std::vector<std::shared_ptr<core::FlowFile>> batched_flowfiles_;
    core::StateManager* state_manager_;
  };

  void processBatch(const std::vector<std::shared_ptr<core::FlowFile>>& batched_flow_files, core::ProcessSession& session);
  std::string createLokiJson(const std::vector<std::shared_ptr<core::FlowFile>>& batched_flow_files, core::ProcessSession& session) const;
  nonstd::expected<void, std::string> submitRequest(const std::string& loki_json);
  void setUpStateManager(core::ProcessContext& context);
  void setUpStreamLableAttributes(core::ProcessContext& context);

  std::optional<uint64_t> max_batch_size_;
  std::map<std::string, std::string> stream_label_attributes_;
  std::vector<std::string> log_line_label_attributes_;
  std::optional<std::string> tenant_id_;
  LogBatch log_batch_;

  curl::HTTPClient client_;
  std::shared_ptr<core::logging::Logger> logger_ = core::logging::LoggerFactory<PushGrafanaLokiREST>::getLogger(uuid_);
};

}  // namespace org::apache::nifi::minifi::extensions::grafana::loki
