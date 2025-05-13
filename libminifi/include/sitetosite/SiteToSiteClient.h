/**
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

#include <algorithm>
#include <array>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "Peer.h"
#include "SiteToSite.h"
#include "core/ProcessSession.h"
#include "core/ProcessContext.h"
#include "core/Connectable.h"
#include "utils/gsl.h"

namespace org::apache::nifi::minifi::sitetosite {

struct DataPacket {
 public:
  DataPacket(std::shared_ptr<Transaction> transaction, std::map<std::string, std::string> attributes, const std::string &payload)
      : attributes{std::move(attributes)},
        transaction{std::move(transaction)},
        payload{payload} {
  }
  std::map<std::string, std::string> attributes;
  uint64_t size{0};
  std::shared_ptr<Transaction> transaction;
  const std::string& payload;
};

class SiteToSiteClient : public core::ConnectableImpl {
 public:
  SiteToSiteClient()
      : core::ConnectableImpl("SitetoSiteClient") {
  }

  ~SiteToSiteClient() override = default;

  virtual bool getPeerList(std::vector<PeerStatus> &peers) = 0;
  virtual bool establish() = 0;
  virtual std::shared_ptr<Transaction> createTransaction(TransferDirection direction) = 0;
  virtual bool transmitPayload(core::ProcessContext& context, core::ProcessSession& session, const std::string &payload,
    std::map<std::string, std::string> attributes) = 0;

  virtual void setPeer(std::unique_ptr<SiteToSitePeer> peer) {
    peer_ = std::move(peer);
  }

  virtual bool bootstrap() {
    return true;
  }

  bool transfer(TransferDirection direction, core::ProcessContext& context, core::ProcessSession& session) {
    if (direction == TransferDirection::SEND) {
      return transferFlowFiles(context, session);
    } else {
      return receiveFlowFiles(context, session);
    }
  }

  bool transferFlowFiles(core::ProcessContext& context, core::ProcessSession& session);
  bool receiveFlowFiles(core::ProcessContext& context, core::ProcessSession& session);
  bool receive(const utils::Identifier &transactionID, DataPacket *packet, bool &eof);

  void setPortId(utils::Identifier &id) {
    port_id_ = id;
  }

  void setIdleTimeout(std::chrono::milliseconds timeout) {
     idle_timeout_ = timeout;
  }

  utils::Identifier getPortId() const {
    return port_id_;
  }

  const std::shared_ptr<core::logging::Logger> &getLogger() {
    return logger_;
  }

  void yield() override {
  }

  bool isRunning() const override {
    return running_;
  }

  bool isWorkAvailable() override {
    return true;
  }

  int16_t send(const utils::Identifier& transactionID, DataPacket* packet, const std::shared_ptr<core::FlowFile>& flowFile, core::ProcessSession* session);

  void setSSLContextService(const std::shared_ptr<minifi::controllers::SSLContextService> &context_service) {
    ssl_context_service_ = context_service;
  }

 protected:
  virtual void tearDown() = 0;
  virtual void deleteTransaction(const utils::Identifier &transactionID);
  virtual int readResponse(const std::shared_ptr<Transaction> &transaction, ResponseCode &code, std::string &message);
  virtual int writeResponse(const std::shared_ptr<Transaction> &transaction, ResponseCode code, const std::string& message);
  virtual const ResponseCodeContext* getRespondCodeContext(ResponseCode code);

  void cancel(const utils::Identifier &transactionID);
  bool complete(core::ProcessContext& context, const utils::Identifier &transactionID);
  void error(const utils::Identifier &transactionID);
  bool confirm(const utils::Identifier &transactionID);

  PeerState peer_state_{PeerState::IDLE};
  utils::Identifier port_id_;
  std::chrono::milliseconds idle_timeout_{15000};
  std::unique_ptr<SiteToSitePeer> peer_;
  std::atomic<bool> running_{false};
  std::map<utils::Identifier, std::shared_ptr<Transaction>> known_transactions_;
  std::chrono::nanoseconds batch_send_nanos_ = std::chrono::seconds(5);

  uint32_t supported_version_[5] = {5, 4, 3, 2, 1};
  uint32_t current_version_index_{0};
  uint32_t current_version_{supported_version_[current_version_index_]};
  uint32_t supported_codec_version_[1] = {1};
  uint32_t current_codec_version_index_{0};
  uint32_t current_codec_version_{supported_codec_version_[current_codec_version_index_]};

  std::shared_ptr<minifi::controllers::SSLContextService> ssl_context_service_;

 private:
  std::shared_ptr<core::logging::Logger> logger_{core::logging::LoggerFactory<SiteToSiteClient>::getLogger()};
};

}  // namespace org::apache::nifi::minifi::sitetosite
