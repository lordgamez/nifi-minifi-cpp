/**
 * @file RemoteProcessorGroupPort.h
 * RemoteProcessorGroupPort class declaration
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
#include <vector>
#include <mutex>
#include <memory>
#include <stack>

#include "http/BaseHTTPClient.h"
#include "concurrentqueue.h"
#include "FlowFileRecord.h"
#include "core/Processor.h"
#include "core/ProcessSession.h"
#include "core/PropertyDefinition.h"
#include "core/PropertyDefinitionBuilder.h"
#include "core/RelationshipDefinition.h"
#include "sitetosite/SiteToSiteClient.h"
#include "minifi-cpp/controllers/SSLContextService.h"
#include "core/logging/LoggerFactory.h"
#include "utils/Export.h"
#include "core/ClassLoader.h"

namespace org::apache::nifi::minifi {

/**
 * Count down latch implementation that's used across
 * all threads of the RPG. This is okay since the latch increments
 * and decrements based on its construction. Using RAII we should
 * never have the concern of thread safety.
 */
class RPGLatch {
 public:
  RPGLatch(bool increment = true) { // NOLINT
    static std::atomic<int64_t> latch_count(0);
    count = &latch_count;
    if (increment)
      count++;
  }

  ~RPGLatch() {
    count--;
  }

  int64_t getCount() {
    return *count;
  }

 private:
  std::atomic<int64_t> *count;
};

struct RPG {
  std::string host_;
  int64_t port_;
  std::string protocol_;
};

class RemoteProcessorGroupPort : public core::ProcessorImpl {
 public:
  RemoteProcessorGroupPort(std::string_view name, std::string url, std::shared_ptr<Configure> configure, const utils::Identifier &uuid = {})
      : core::ProcessorImpl(name, uuid),
        configure_(std::move(configure)),
        direction_(sitetosite::TransferDirection::SEND),
        transmitting_(false),
        ssl_service_(nullptr),
        logger_(core::logging::LoggerFactory<RemoteProcessorGroupPort>::getLogger(uuid)) {
    client_type_ = sitetosite::ClientType::RAW;
    protocol_uuid_ = uuid;
    peer_index_ = -1;
    // REST API port and host
    setURL(std::move(url));
  }
  virtual ~RemoteProcessorGroupPort() = default;

  MINIFIAPI static constexpr auto hostName = core::PropertyDefinitionBuilder<>::createProperty("Host Name")
      .withDescription("Remote Host Name.")
      .build();
  MINIFIAPI static constexpr auto SSLContext = core::PropertyDefinitionBuilder<>::createProperty("SSL Context Service")
      .withDescription("The SSL Context Service used to provide client certificate information for TLS/SSL (https) connections.")
      .build();
  MINIFIAPI static constexpr auto port = core::PropertyDefinitionBuilder<>::createProperty("Port")
      .withDescription("Remote Port")
      .build();
  MINIFIAPI static constexpr auto portUUID = core::PropertyDefinitionBuilder<>::createProperty("Port UUID")
      .withDescription("Specifies remote NiFi Port UUID.")
      .build();
  MINIFIAPI static constexpr auto idleTimeout = core::PropertyDefinitionBuilder<>::createProperty("Idle Timeout")
    .withDescription("Max idle time for remote service")
    .isRequired(true)
    .withValidator(core::StandardPropertyValidators::TIME_PERIOD_VALIDATOR)
    .withDefaultValue("15 s")
    .build();

  MINIFIAPI static constexpr auto Properties = std::to_array<core::PropertyReference>({
      hostName,
      SSLContext,
      port,
      portUUID,
      idleTimeout
  });


  MINIFIAPI static constexpr auto DefaultRelationship = core::RelationshipDefinition{"", ""};
  MINIFIAPI static constexpr auto Relationships = std::array{DefaultRelationship};

  MINIFIAPI static constexpr bool SupportsDynamicProperties = false;
  MINIFIAPI static constexpr bool SupportsDynamicRelationships = false;
  MINIFIAPI static constexpr core::annotation::Input InputRequirement = core::annotation::Input::INPUT_ALLOWED;
  MINIFIAPI static constexpr bool IsSingleThreaded = false;

  ADD_COMMON_VIRTUAL_FUNCTIONS_FOR_PROCESSORS

  void onSchedule(core::ProcessContext& context, core::ProcessSessionFactory& session_factory) override;
  void onTrigger(core::ProcessContext& context, core::ProcessSession& session) override;
  void initialize() override;

  void setDirection(sitetosite::TransferDirection direction) {
    direction_ = direction;
    if (direction_ == sitetosite::TransferDirection::RECEIVE)
      this->setTriggerWhenEmpty(true);
  }

  void setTimeout(std::chrono::milliseconds timeout) {
    timeout_ = timeout;
  }

  void setTransmitting(bool val) {
    transmitting_ = val;
  }

  void setInterface(const std::string &ifc) {
    local_network_interface_ = ifc;
  }

  std::string getInterface() {
    return local_network_interface_;
  }

  void setURL(std::string val);

  std::vector<RPG> getInstances() const {
    return nifi_instances_;
  }

  void setHTTPProxy(const http::HTTPProxy &proxy) {
    proxy_ = proxy;
  }

  http::HTTPProxy getHTTPProxy() {
    return proxy_;
  }

  std::pair<std::string, int> refreshRemoteSite2SiteInfo();
  void refreshPeerList();
  void notifyStop() override;

  void enableHTTP() {
    client_type_ = sitetosite::ClientType::HTTP;
  }

  void setUseCompression(bool use_compression) {
    use_compression_ = use_compression;
  }

  bool getUseCompression() const {
    return use_compression_;
  }

  void setBatchCount(uint64_t count) {
    batch_count_ = count;
  }

  std::optional<uint64_t> getBatchCount() const {
    return batch_count_;
  }

  void setBatchSize(uint64_t size) {
    batch_size_ = size;
  }

  std::optional<uint64_t> getBatchSize() const {
    return batch_size_;
  }

  void setBatchDuration(std::chrono::milliseconds duration) {
    batch_duration_ = duration;
  }

  std::optional<std::chrono::milliseconds> getBatchDuration() const {
    return batch_duration_;
  }

 protected:
  std::unique_ptr<sitetosite::SiteToSiteClient> getNextProtocol();
  void returnProtocol(std::unique_ptr<sitetosite::SiteToSiteClient> protocol);

  moodycamel::ConcurrentQueue<std::unique_ptr<sitetosite::SiteToSiteClient>> available_protocols_;
  std::shared_ptr<Configure> configure_;
  sitetosite::TransferDirection direction_;
  std::atomic<bool> transmitting_;
  std::optional<std::chrono::milliseconds> timeout_;
  std::string local_network_interface_;
  utils::Identifier protocol_uuid_;
  std::chrono::milliseconds idle_timeout_ = std::chrono::seconds(15);
  std::vector<RPG> nifi_instances_;
  http::HTTPProxy proxy_;
  sitetosite::ClientType client_type_;
  std::vector<sitetosite::PeerStatus> peers_;
  std::atomic<int64_t> peer_index_;
  std::mutex peer_mutex_;
  std::string rest_user_name_;
  std::string rest_password_;
  std::shared_ptr<controllers::SSLContextService> ssl_service_;
  bool use_compression_{false};
  std::optional<uint64_t> batch_count_;
  std::optional<uint64_t> batch_size_;
  std::optional<std::chrono::milliseconds> batch_duration_;

 private:
  std::unique_ptr<sitetosite::SiteToSiteClient> initializeProtocol(sitetosite::SiteToSiteClientConfiguration& config);

  std::shared_ptr<core::logging::Logger> logger_;
  static const char* RPG_SSL_CONTEXT_SERVICE_NAME;
};

}  // namespace org::apache::nifi::minifi
