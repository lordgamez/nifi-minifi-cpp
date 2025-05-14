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

#include <algorithm>
#include <map>
#include <memory>
#include <string>
#include <utility>

#include "io/BaseStream.h"
#include "sitetosite/Peer.h"
#include "sitetosite/RawSocketProtocol.h"
#include "unit/TestBase.h"
#include "unit/Catch.h"
#include "unit/SiteToSiteHelper.h"

#define FMT_DEFAULT fmt_lower

TEST_CASE("TestSetPortId", "[S2S]") {
  auto peer = std::make_unique<minifi::sitetosite::SiteToSitePeer>(std::make_unique<org::apache::nifi::minifi::io::BufferStream>(), "fake_host", 65433, "");

  minifi::sitetosite::RawSiteToSiteClient protocol(std::move(peer));

  utils::Identifier fakeUUID = utils::Identifier::parse("c56a4180-65aa-42ec-a945-5fd21dec0538").value();

  protocol.setPortId(fakeUUID);

  REQUIRE(fakeUUID == protocol.getPortId());
}

void sunny_path_bootstrap(const std::unique_ptr<SiteToSiteResponder>& collector) {
  char a = 0x14;  // RESOURCE_OK
  std::string resp_code;
  resp_code.insert(resp_code.begin(), a);
  collector->push_response(resp_code);

  // Handshake response code
  resp_code = "R";
  collector->push_response(resp_code);
  resp_code = "C";
  collector->push_response(resp_code);
  char b = 0x1;
  resp_code = b;
  collector->push_response(resp_code);

  // Codec Negotiation
  resp_code = a;
  collector->push_response(resp_code);
}

TEST_CASE("TestSiteToSiteVerifySend", "[S2S]") {
  auto collector = std::make_unique<SiteToSiteResponder>();
  auto collector_ptr = collector.get();

  sunny_path_bootstrap(collector);

  auto peer = std::make_unique<minifi::sitetosite::SiteToSitePeer>(std::move(collector), "fake_host", 65433, "");

  minifi::sitetosite::RawSiteToSiteClient protocol(std::move(peer));
  protocol.setUseCompression(true);
  protocol.setBatchDuration(std::chrono::milliseconds(100));
  protocol.setBatchCount(5);

  utils::Identifier fakeUUID = utils::Identifier::parse("C56A4180-65AA-42EC-A945-5FD21DEC0538").value();

  protocol.setPortId(fakeUUID);

  REQUIRE(true == protocol.bootstrap());

  REQUIRE(collector_ptr->get_next_client_response() == "NiFi");
  collector_ptr->get_next_client_response();
  REQUIRE(collector_ptr->get_next_client_response() == "SocketFlowFileProtocol");
  collector_ptr->get_next_client_response();
  collector_ptr->get_next_client_response();
  collector_ptr->get_next_client_response();
  collector_ptr->get_next_client_response();
  REQUIRE(collector_ptr->get_next_client_response() == "nifi://fake_host:65433");
  collector_ptr->get_next_client_response();
  collector_ptr->get_next_client_response();
  REQUIRE(collector_ptr->get_next_client_response() == "BATCH_COUNT");
  collector_ptr->get_next_client_response();
  REQUIRE(collector_ptr->get_next_client_response() == "5");
  collector_ptr->get_next_client_response();
  REQUIRE(collector_ptr->get_next_client_response() == "BATCH_DURATION");
  collector_ptr->get_next_client_response();
  REQUIRE(collector_ptr->get_next_client_response() == "100");
  collector_ptr->get_next_client_response();
  REQUIRE(collector_ptr->get_next_client_response() == "GZIP");
  collector_ptr->get_next_client_response();
  REQUIRE(collector_ptr->get_next_client_response() == "true");
  collector_ptr->get_next_client_response();
  REQUIRE(collector_ptr->get_next_client_response() == "PORT_IDENTIFIER");
  collector_ptr->get_next_client_response();
  REQUIRE(utils::string::equalsIgnoreCase(collector_ptr->get_next_client_response(), "c56a4180-65aa-42ec-a945-5fd21dec0538"));
  collector_ptr->get_next_client_response();
  REQUIRE(collector_ptr->get_next_client_response() == "REQUEST_EXPIRATION_MILLIS");
  collector_ptr->get_next_client_response();
  REQUIRE(collector_ptr->get_next_client_response() == "30000");
  collector_ptr->get_next_client_response();
  REQUIRE(collector_ptr->get_next_client_response() == "NEGOTIATE_FLOWFILE_CODEC");
  collector_ptr->get_next_client_response();
  REQUIRE(collector_ptr->get_next_client_response() == "StandardFlowFileCodec");
  collector_ptr->get_next_client_response();  // codec version

  // start to send the stuff
  // Create the transaction
  std::string payload = "Test MiNiFi payload";
  std::shared_ptr<minifi::sitetosite::Transaction> transaction;
  transaction = protocol.createTransaction(minifi::sitetosite::TransferDirection::SEND);
  REQUIRE(transaction);
  auto transactionID = transaction->getUUID();
  collector_ptr->get_next_client_response();
  REQUIRE(collector_ptr->get_next_client_response() == "SEND_FLOWFILES");
  std::map<std::string, std::string> attributes;
  minifi::sitetosite::DataPacket packet(transaction, attributes, payload);
  REQUIRE(protocol.send(transactionID, &packet, nullptr, nullptr));
  collector_ptr->get_next_client_response();
  collector_ptr->get_next_client_response();
  std::string rx_payload = collector_ptr->get_next_client_response();
  REQUIRE(payload == rx_payload);
}

TEST_CASE("TestSiteToSiteVerifyNegotiationFail", "[S2S]") {
  auto collector = std::make_unique<SiteToSiteResponder>();

  char a = '\xFF';
  std::string resp_code;
  resp_code.insert(resp_code.begin(), a);
  collector->push_response(resp_code);
  collector->push_response(resp_code);

  auto peer = std::make_unique<minifi::sitetosite::SiteToSitePeer>(std::move(collector), "fake_host", 65433, "");

  minifi::sitetosite::RawSiteToSiteClient protocol(std::move(peer));

  std::string uuid_str = "C56A4180-65AA-42EC-A945-5FD21DEC0538";

  utils::Identifier fakeUUID;

  fakeUUID = uuid_str;

  protocol.setPortId(fakeUUID);

  REQUIRE(false == protocol.bootstrap());
}

TEST_CASE("Test receiving data through site to site ", "[S2S]") {
  auto collector = std::make_unique<SiteToSiteResponder>();
  auto collector_ptr = collector.get();

  sunny_path_bootstrap(collector);
  collector->push_response("R");
  collector->push_response("C");
  auto addChar = [&collector](char ch) {
    std::string resp_code;
    resp_code.insert(resp_code.begin(), ch);
    collector->push_response(resp_code);
  };
  addChar(0x14);  // MORE_DATA
  auto addInt32 = [&collector](uint32_t number) {
    std::string result(4, '\0');
    for (std::size_t i = 0; i < 4; ++i) {
      result[i] = static_cast<char>((number >> (8 * (3 - i))) & 0xFF);
    }
    collector->push_response(result);
  };
  addInt32(1);  // number of attributes
  addInt32(13);  // number of bytes
  collector->push_response("attribute_key");
  addInt32(15);  // number of bytes
  collector->push_response("attribute_value");
  auto addInt64 = [&collector](uint64_t number) {
    std::string result(8, '\0');
    for (std::size_t i = 0; i < 8; ++i) {
      result[i] = static_cast<char>((number >> (8 * (7 - i))) & 0xFF);
    }
    collector->push_response(result);
  };
  addInt64(4);  // payload length
  collector->push_response("data");

  auto peer = std::make_unique<minifi::sitetosite::SiteToSitePeer>(std::move(collector), "fake_host", 65433, "");

  minifi::sitetosite::RawSiteToSiteClient protocol(std::move(peer));

  utils::Identifier fakeUUID = utils::Identifier::parse("C56A4180-65AA-42EC-A945-5FD21DEC0538").value();

  protocol.setPortId(fakeUUID);

  REQUIRE(true == protocol.bootstrap());

  REQUIRE(collector_ptr->get_next_client_response() == "NiFi");
  collector_ptr->get_next_client_response();
  REQUIRE(collector_ptr->get_next_client_response() == "SocketFlowFileProtocol");
  collector_ptr->get_next_client_response();
  collector_ptr->get_next_client_response();
  collector_ptr->get_next_client_response();
  collector_ptr->get_next_client_response();
  REQUIRE(collector_ptr->get_next_client_response() == "nifi://fake_host:65433");
  collector_ptr->get_next_client_response();
  collector_ptr->get_next_client_response();
  REQUIRE(collector_ptr->get_next_client_response() == "GZIP");
  collector_ptr->get_next_client_response();
  REQUIRE(collector_ptr->get_next_client_response() == "false");
  collector_ptr->get_next_client_response();
  REQUIRE(collector_ptr->get_next_client_response() == "PORT_IDENTIFIER");
  collector_ptr->get_next_client_response();
  REQUIRE(utils::string::equalsIgnoreCase(collector_ptr->get_next_client_response(), "c56a4180-65aa-42ec-a945-5fd21dec0538"));
  collector_ptr->get_next_client_response();
  REQUIRE(collector_ptr->get_next_client_response() == "REQUEST_EXPIRATION_MILLIS");
  collector_ptr->get_next_client_response();
  REQUIRE(collector_ptr->get_next_client_response() == "30000");
  collector_ptr->get_next_client_response();
  REQUIRE(collector_ptr->get_next_client_response() == "NEGOTIATE_FLOWFILE_CODEC");
  collector_ptr->get_next_client_response();
  REQUIRE(collector_ptr->get_next_client_response() == "StandardFlowFileCodec");
  collector_ptr->get_next_client_response();  // codec version

  std::shared_ptr<minifi::sitetosite::Transaction> transaction;
  transaction = protocol.createTransaction(minifi::sitetosite::TransferDirection::RECEIVE);
  REQUIRE(transaction);
  auto transactionID = transaction->getUUID();
  collector_ptr->get_next_client_response();
  REQUIRE(collector_ptr->get_next_client_response() == "RECEIVE_FLOWFILES");
  std::map<std::string, std::string> attributes;
  minifi::sitetosite::DataPacket packet(transaction, attributes, "");
  bool eof = false;
  REQUIRE(protocol.receive(transactionID, &packet, eof));
}
