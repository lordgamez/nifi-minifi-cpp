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
  static constexpr char const* PROTOCOL_VERSION_HEADER = "x-nifi-site-to-site-protocol-version";

 public:
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

  bool getPeerList(std::vector<PeerStatus> &peers) override;

  /**
   * Establish the protocol connection. Not needed for the HTTP connection, so we simply
   * return true.
   */
  bool establish() override {
    return true;
  }

  std::shared_ptr<Transaction> createTransaction(TransferDirection direction) override;

  //! Transfer string for the process session
  bool transmitPayload(core::ProcessContext& context, core::ProcessSession& session, const std::string &payload,
                               std::map<std::string, std::string> attributes) override;
  void deleteTransaction(const utils::Identifier& transaction_id) override;

 protected:
  void closeTransaction(const utils::Identifier &transaction_id);
  int readResponse(const std::shared_ptr<Transaction> &transaction, ResponseCode &code, std::string &message) override;
  int writeResponse(const std::shared_ptr<Transaction> &transaction, ResponseCode code, const std::string& message) override;

  /**
   * Bootstrapping is not really required for the HTTP Site To Site so we will set the peer state and return true.
   */
  bool bootstrap() override {
    peer_state_ = PeerState::READY;
    return true;
  }

  /**
   * Creates a connection for sending, returning an HTTP client that is structured and configured
   * to receive flow files.
   * @param transaction transaction against which we are performing our sends
   * @return HTTP Client shared pointer.
   */
  std::shared_ptr<minifi::http::HTTPClient> openConnectionForSending(const std::shared_ptr<HttpTransaction> &transaction);

  /**
   * Creates a connection for receiving, returning an HTTP client that is structured and configured
   * to receive flow files.
   * @param transaction transaction against which we are performing our reads
   * @return HTTP Client shared pointer.
   */
  std::shared_ptr<minifi::http::HTTPClient> openConnectionForReceive(const std::shared_ptr<HttpTransaction> &transaction);

  std::string getBaseURI();

  void tearDown() override;

  std::unique_ptr<minifi::http::HTTPClient> createHttpClient(const std::string &uri, http::HttpRequestMethod method);

 private:
  ResponseCode current_code_;
  std::shared_ptr<core::logging::Logger> logger_ = core::logging::LoggerFactory<HttpSiteToSiteClient>::getLogger();

  HttpSiteToSiteClient(const HttpSiteToSiteClient &parent);
  HttpSiteToSiteClient &operator=(const HttpSiteToSiteClient &parent);
};

}  // namespace org::apache::nifi::minifi::sitetosite
