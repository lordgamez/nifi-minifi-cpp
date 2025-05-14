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

#include <map>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>
#include "HTTPTransaction.h"
#include "sitetosite/SiteToSite.h"
#include "sitetosite/SiteToSiteClient.h"
#include "core/logging/LoggerConfiguration.h"
#include "core/ProcessContext.h"
#include "core/ProcessSession.h"
#include "sitetosite/Peer.h"
#include "utils/Id.h"

namespace org::apache::nifi::minifi::sitetosite {

class HttpSiteToSiteClient : public SiteToSiteClient {
 public:
  static constexpr char const* PROTOCOL_VERSION_HEADER = "x-nifi-site-to-site-protocol-version";
  static constexpr char const* HANDSHAKE_PROPERTY_USE_COMPRESSION = "x-nifi-site-to-site-use-compression";
  static constexpr char const* HANDSHAKE_PROPERTY_REQUEST_EXPIRATION = "x-nifi-site-to-site-request-expiration";
  static constexpr char const* HANDSHAKE_PROPERTY_BATCH_COUNT = "x-nifi-site-to-site-batch-count";
  static constexpr char const* HANDSHAKE_PROPERTY_BATCH_SIZE = "x-nifi-site-to-site-batch-size";
  static constexpr char const* HANDSHAKE_PROPERTY_BATCH_DURATION = "x-nifi-site-to-site-batch-duration";

  explicit HttpSiteToSiteClient(std::string /*name*/, const utils::Identifier& /*uuid*/ = {})
      : SiteToSiteClient(),
        current_code_(ResponseCode::UNRECOGNIZED_RESPONSE_CODE) {
    peer_state_ = PeerState::READY;
  }

  explicit HttpSiteToSiteClient(std::unique_ptr<SiteToSitePeer> peer)
      : SiteToSiteClient(),
        current_code_(ResponseCode::UNRECOGNIZED_RESPONSE_CODE) {
    peer_ = std::move(peer);
    peer_state_ = PeerState::READY;
  }
  ~HttpSiteToSiteClient() override = default;

  MINIFIAPI static constexpr auto Properties = std::array<core::PropertyReference, 0>{};
  MINIFIAPI static constexpr bool SupportsDynamicProperties = false;
  MINIFIAPI static constexpr bool SupportsDynamicRelationships = false;

  void setPeer(std::unique_ptr<SiteToSitePeer> peer) override {
    peer_ = std::move(peer);
  }

  std::optional<std::vector<PeerStatus>> getPeerList() override;

  bool establish() override {
    return true;
  }

  std::shared_ptr<Transaction> createTransaction(TransferDirection direction) override;

  bool transmitPayload(core::ProcessContext& context, core::ProcessSession& session, const std::string &payload, const std::map<std::string, std::string>& attributes) override;
  void deleteTransaction(const utils::Identifier& transaction_id) override;

 protected:
  void closeTransaction(const utils::Identifier &transaction_id);
  std::optional<SiteToSiteResponse> readResponse(const std::shared_ptr<Transaction> &transaction) override;
  bool writeResponse(const std::shared_ptr<Transaction> &transaction, const SiteToSiteResponse& response) override;

  bool bootstrap() override {
    peer_state_ = PeerState::READY;
    return true;
  }

  std::shared_ptr<minifi::http::HTTPClient> openConnectionForSending(const std::shared_ptr<HttpTransaction> &transaction);
  std::shared_ptr<minifi::http::HTTPClient> openConnectionForReceive(const std::shared_ptr<HttpTransaction> &transaction);

  std::string getBaseURI();

  void tearDown() override;

  std::unique_ptr<minifi::http::HTTPClient> createHttpClient(const std::string &uri, http::HttpRequestMethod method);

 private:
  void setSiteToSiteHeaders(minifi::http::HTTPClient& client);

  ResponseCode current_code_;
  std::shared_ptr<core::logging::Logger> logger_ = core::logging::LoggerFactory<HttpSiteToSiteClient>::getLogger();

  HttpSiteToSiteClient(const HttpSiteToSiteClient &parent);
  HttpSiteToSiteClient &operator=(const HttpSiteToSiteClient &parent);
};

}  // namespace org::apache::nifi::minifi::sitetosite
