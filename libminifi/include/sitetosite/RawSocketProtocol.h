/**
 * @file RawSiteToSiteClient.h
 * RawSiteToSiteClient class declaration
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

#include <errno.h>
#include <stdio.h>
#include <sys/types.h>

#include <algorithm>
#include <chrono>
#include <map>
#include <memory>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include "core/logging/LoggerFactory.h"
#include "core/ProcessContext.h"
#include "core/ProcessSession.h"
#include "core/Property.h"
#include "FlowFileRecord.h"
#include "io/CRCStream.h"
#include "Peer.h"
#include "properties/Configure.h"
#include "SiteToSite.h"
#include "SiteToSiteClient.h"
#include "utils/Id.h"

namespace org::apache::nifi::minifi::sitetosite {

class RawSiteToSiteClient : public sitetosite::SiteToSiteClient {
 public:
  static const char *HandShakePropertyStr[MAX_HANDSHAKE_PROPERTY];

  explicit RawSiteToSiteClient(std::unique_ptr<SiteToSitePeer> peer) {
    peer_ = std::move(peer);
    batch_size_ = 0;
    batch_count_ = 0;
    batch_duration_ = std::chrono::seconds(0);
    batch_send_nanos_ = std::chrono::seconds(5);
    timeout_ = std::chrono::seconds(30);
    supported_version_[0] = 5;
    supported_version_[1] = 4;
    supported_version_[2] = 3;
    supported_version_[3] = 2;
    supported_version_[4] = 1;
    current_version_ = supported_version_[0];
    current_version_index_ = 0;
    supported_codec_version_[0] = 1;
    current_codec_version_ = supported_codec_version_[0];
    current_codec_version_index_ = 0;
  }

  RawSiteToSiteClient(const RawSiteToSiteClient &parent) = delete;
  RawSiteToSiteClient &operator=(const RawSiteToSiteClient &parent) = delete;
  RawSiteToSiteClient(RawSiteToSiteClient &&parent) = delete;
  RawSiteToSiteClient &operator=(RawSiteToSiteClient &&parent) = delete;

  ~RawSiteToSiteClient() override {
    tearDown();
  }

 public:
  // setBatchSize
  void setBatchSize(uint64_t size) {
    batch_size_ = size;
  }
  // setBatchCount
  void setBatchCount(uint64_t count) {
    batch_count_ = count;
  }
  // setBatchDuration
  void setBatchDuration(std::chrono::milliseconds duration) {
    batch_duration_ = duration;
  }
  // setTimeout
  void setTimeout(std::chrono::milliseconds time) {
    timeout_ = time;
    if (peer_)
      peer_->setTimeout(time);
  }

  /**
   * Provides a reference to the time out
   * @returns timeout
   */
  std::chrono::milliseconds getTimeout() const {
    return timeout_;
  }

  // getResourceName
  std::string getResourceName() {
    return "SocketFlowFileProtocol";
  }
  // getCodecResourceName
  std::string getCodecResourceName() {
    return "StandardFlowFileCodec";
  }

  // get peerList
  bool getPeerList(std::vector<PeerStatus> &peer) override;
  // negotiateCodec
  virtual bool negotiateCodec();
  // initiateResourceNegotiation
  virtual bool initiateResourceNegotiation();
  // initiateCodecResourceNegotiation
  virtual bool initiateCodecResourceNegotiation();
  // tearDown
  void tearDown() override;
  // write Request Type
  virtual int writeRequestType(RequestType type);
  // read Request Type
  virtual int readRequestType(RequestType &type);

  // read Respond
  virtual int readRespond(const std::shared_ptr<Transaction> &transaction, ResponseCode &code, std::string &message);
  // write respond
  virtual int writeRespond(const std::shared_ptr<Transaction> &transaction, ResponseCode code, const std::string& message);
  // getRespondCodeContext
  RespondCodeContext *getRespondCodeContext(ResponseCode code) override {
    return SiteToSiteClient::getRespondCodeContext(code);
  }

  // Creation of a new transaction, return the transaction ID if success,
  // Return NULL when any error occurs
  std::shared_ptr<Transaction> createTransaction(TransferDirection direction) override;

  //! Transfer string for the process session
  bool transmitPayload(core::ProcessContext& context, core::ProcessSession& session, const std::string &payload,
      std::map<std::string, std::string> attributes) override;

  // bootstrap the protocol to the ready for transaction state by going through the state machine
  bool bootstrap() override;

 protected:
  // establish
  bool establish() override;
  // handShake
  virtual bool handShake();

 private:
  // Logger
  std::shared_ptr<core::logging::Logger> logger_ = core::logging::LoggerFactory<RawSiteToSiteClient>::getLogger();
  // Batch Count
  std::atomic<uint64_t> batch_count_;
  // Batch Size
  std::atomic<uint64_t> batch_size_;
  // Batch Duration in msec
  std::atomic<std::chrono::milliseconds> batch_duration_;
  // Timeout in msec
  std::atomic<std::chrono::milliseconds> timeout_;

  // commsIdentifier
  utils::Identifier _commsIdentifier;

  static std::shared_ptr<utils::IdGenerator> id_generator_;
};

}  // namespace org::apache::nifi::minifi::sitetosite
