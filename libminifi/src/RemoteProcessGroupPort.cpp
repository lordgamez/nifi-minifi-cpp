/**
 * @file RemoteProcessGroupPort.cpp
 * RemoteProcessGroupPort class implementation
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

#include "RemoteProcessGroupPort.h"

#include <memory>
#include <iostream>
#include <vector>
#include <string>
#include <utility>
#include <cinttypes>

#include "sitetosite/Peer.h"
#include "Exception.h"
#include "sitetosite/SiteToSiteFactory.h"

#include "rapidjson/document.h"

#include "core/logging/Logger.h"
#include "core/ProcessContext.h"
#include "core/Processor.h"
#include "http/BaseHTTPClient.h"
#include "controllers/SSLContextService.h"
#include "utils/net/DNS.h"

#undef GetObject  // windows.h #defines GetObject = GetObjectA or GetObjectW, which conflicts with rapidjson

using namespace std::literals::chrono_literals;

namespace org::apache::nifi::minifi {

const char *RemoteProcessGroupPort::RPG_SSL_CONTEXT_SERVICE_NAME = "RemoteProcessGroupPortSSLContextService";

void RemoteProcessGroupPort::setURL(std::string val) {
  auto urls = utils::string::split(val, ",");
  for (const auto& url : urls) {
    http::URL parsed_url{utils::string::trim(url)};
    if (parsed_url.isValid()) {
      logger_->log_debug("Parsed RPG URL '{}' -> '{}'", url, parsed_url.hostPort());
      nifi_instances_.push_back({parsed_url.host(), parsed_url.port(), parsed_url.protocol()});
    } else {
      logger_->log_error("Could not parse RPG URL '{}'", url);
    }
  }
}

std::unique_ptr<sitetosite::SiteToSiteClient> RemoteProcessGroupPort::initializeProtocol(sitetosite::SiteToSiteClientConfiguration& config) {
  config.setSecurityContext(ssl_service_);
  config.setHTTPProxy(proxy_);
  config.setIdleTimeout(idle_timeout_);
  config.setUseCompression(use_compression_);
  config.setBatchCount(batch_count_);
  config.setBatchSize(batch_size_);
  config.setBatchDuration(batch_duration_);
  config.setTimeout(timeout_);

  return sitetosite::createClient(config);
}

std::unique_ptr<sitetosite::SiteToSiteClient> RemoteProcessGroupPort::getNextProtocol() {
  std::unique_ptr<sitetosite::SiteToSiteClient> nextProtocol = nullptr;
  if (!available_protocols_.try_dequeue(nextProtocol)) {
    if (peer_index_ >= 0) {
      std::lock_guard<std::mutex> lock(peer_mutex_);
      logger_->log_debug("Creating client from peer {}", peer_index_.load());
      auto& peer_status = peers_[peer_index_];
      sitetosite::SiteToSiteClientConfiguration config(peer_status.getPortId(), peer_status.getHost(), peer_status.getPort(), local_network_interface_, client_type_);
      peer_index_++;
      if (peer_index_ >= static_cast<int>(peers_.size())) {
        peer_index_ = 0;
      }
      nextProtocol = initializeProtocol(config);
    } else {
      logger_->log_debug("Refreshing the peer list since there are none configured.");
      refreshPeerList();
    }
  }
  logger_->log_debug("Obtained protocol from available_protocols_");
  return nextProtocol;
}

void RemoteProcessGroupPort::returnProtocol(std::unique_ptr<sitetosite::SiteToSiteClient> return_protocol) {
  auto count = peers_.size();
  if (max_concurrent_tasks_ > count)
    count = max_concurrent_tasks_;
  if (available_protocols_.size_approx() >= count) {
    logger_->log_debug("not enqueueing protocol {}", getUUIDStr());
    // let the memory be freed
    return;
  }
  logger_->log_debug("enqueueing protocol {}, have a total of {}", getUUIDStr(), available_protocols_.size_approx());
  available_protocols_.enqueue(std::move(return_protocol));
}

void RemoteProcessGroupPort::initialize() {
  setSupportedProperties(Properties);
  setSupportedRelationships(Relationships);

  logger_->log_trace("Finished initialization");
}

void RemoteProcessGroupPort::onSchedule(core::ProcessContext& context, core::ProcessSessionFactory&) {
  if (auto protocol_uuid = context.getProperty(portUUID)) {
    protocol_uuid_ = *protocol_uuid;
  }

  auto context_name = context.getProperty(SSLContext);
  if (!context_name || IsNullOrEmpty(*context_name)) {
    context_name = RPG_SSL_CONTEXT_SERVICE_NAME;
  }

  std::shared_ptr<core::controller::ControllerService> service = context.getControllerService(*context_name, getUUID());
  if (nullptr != service) {
    ssl_service_ = std::dynamic_pointer_cast<minifi::controllers::SSLContextService>(service);
  } else {
    std::string secureStr;
    if (configure_->get(Configure::nifi_remote_input_secure, secureStr) && utils::string::toBool(secureStr).value_or(false)) {
      ssl_service_ = std::make_shared<minifi::controllers::SSLContextServiceImpl>(RPG_SSL_CONTEXT_SERVICE_NAME, configure_);
      ssl_service_->onEnable();
    }
  }

  idle_timeout_ = context.getProperty(idleTimeout) | utils::andThen(parsing::parseDuration<std::chrono::milliseconds>) | utils::orThrow("RemoteProcessGroupPort::idleTimeout is a required Property");

  std::lock_guard<std::mutex> lock(peer_mutex_);
  if (!nifi_instances_.empty()) {
    refreshPeerList();
    if (!peers_.empty())
      peer_index_ = 0;
  }
  // populate the site2site protocol for load balancing between them
  if (!peers_.empty()) {
    auto count = peers_.size();
    if (max_concurrent_tasks_ > count)
      count = max_concurrent_tasks_;
    for (uint32_t i = 0; i < count; i++) {
      auto peer_status = peers_[peer_index_];
      sitetosite::SiteToSiteClientConfiguration config(peer_status.getPortId(), peer_status.getHost(), peer_status.getPort(), getInterface(), client_type_);
      peer_index_++;
      if (peer_index_ >= static_cast<int>(peers_.size())) {
        peer_index_ = 0;
      }
      logger_->log_trace("Creating client");
      auto nextProtocol = initializeProtocol(config);
      logger_->log_trace("Created client, moving into available protocols");
      returnProtocol(std::move(nextProtocol));
    }
  } else {
    // we don't have any peers
    logger_->log_error("No peers selected during scheduling");
  }
}

void RemoteProcessGroupPort::notifyStop() {
  transmitting_ = false;
  RPGLatch count(false);  // we're just a monitor
  // we use the latch
  while (count.getCount() > 0) {
  }
  std::unique_ptr<sitetosite::SiteToSiteClient> nextProtocol = nullptr;
  while (available_protocols_.try_dequeue(nextProtocol)) {
    // clear all protocols now
  }
}

void RemoteProcessGroupPort::onTrigger(core::ProcessContext& context, core::ProcessSession& session) {
  logger_->log_trace("On trigger {}", getUUIDStr());
  if (!transmitting_) {
    return;
  }

  RPGLatch count;

  std::string value;

  logger_->log_trace("On trigger {}", getUUIDStr());

  std::unique_ptr<sitetosite::SiteToSiteClient> protocol_ = nullptr;
  try {
    logger_->log_trace("get protocol in on trigger");
    protocol_ = getNextProtocol();

    if (!protocol_) {
      logger_->log_info("no protocol, yielding");
      context.yield();
      return;
    }

    if (!protocol_->transfer(direction_, context, session)) {
      logger_->log_warn("protocol transmission failed, yielding");
      context.yield();
    }

    returnProtocol(std::move(protocol_));
    return;
  } catch (const std::exception&) {
    context.yield();
    session.rollback();
  }
}

std::pair<std::string, int> RemoteProcessGroupPort::refreshRemoteSite2SiteInfo() {
  if (nifi_instances_.empty())
    return std::make_pair("", -1);

  for (const auto& nifi : nifi_instances_) {
    std::string host = nifi.host_;
#ifdef WIN32
    if ("localhost" == host) {
      host = org::apache::nifi::minifi::utils::net::getMyHostName();
    }
#endif
    std::string protocol = nifi.protocol_;
    int nifi_port = nifi.port_;
    std::stringstream fullUrl;
    fullUrl << protocol << host;
    // don't append port if it is 0 ( undefined )
    if (nifi_port > 0) {
      fullUrl << ":" << std::to_string(nifi_port);
    }
    fullUrl << "/nifi-api/site-to-site";

    configure_->get(Configure::nifi_rest_api_user_name, rest_user_name_);
    configure_->get(Configure::nifi_rest_api_password, rest_password_);

    std::string token;
    std::unique_ptr<http::BaseHTTPClient> client;
    if (!rest_user_name_.empty()) {
      std::stringstream loginUrl;
      loginUrl << protocol << host;
      // don't append port if it is 0 ( undefined )
      if (nifi_port > 0) {
        loginUrl << ":" << std::to_string(nifi_port);
      }
      loginUrl << "/nifi-api/access/token";

      auto client_ptr = core::ClassLoader::getDefaultClassLoader().instantiateRaw("HTTPClient", "HTTPClient");
      if (nullptr == client_ptr) {
        logger_->log_error("Could not locate HTTPClient. You do not have cURL support!");
        return std::make_pair("", -1);
      }
      client = std::unique_ptr<http::BaseHTTPClient>(dynamic_cast<http::BaseHTTPClient*>(client_ptr));
      client->initialize(http::HttpRequestMethod::GET, loginUrl.str(), ssl_service_);
      // use a connection timeout. if this times out we will simply attempt re-connection
      // so no need for configuration parameter that isn't already defined in Processor
      client->setConnectionTimeout(10s);
      client->setReadTimeout(idle_timeout_);

      token = http::get_token(client.get(), rest_user_name_, rest_password_);
      logger_->log_debug("Token from NiFi REST Api endpoint {},  {}", loginUrl.str(), token);
      if (token.empty())
        return std::make_pair("", -1);
    }

    auto client_ptr = core::ClassLoader::getDefaultClassLoader().instantiateRaw("HTTPClient", "HTTPClient");
    if (nullptr == client_ptr) {
      logger_->log_error("Could not locate HTTPClient. You do not have cURL support, defaulting to base configuration!");
      return std::make_pair("", -1);
    }
    int siteTosite_port = -1;
    client = std::unique_ptr<http::BaseHTTPClient>(dynamic_cast<http::BaseHTTPClient*>(client_ptr));
    client->initialize(http::HttpRequestMethod::GET, fullUrl.str(), ssl_service_);
    // use a connection timeout. if this times out we will simply attempt re-connection
    // so no need for configuration parameter that isn't already defined in Processor
    client->setConnectionTimeout(10s);
    client->setReadTimeout(idle_timeout_);
    if (!proxy_.host.empty()) {
      client->setHTTPProxy(proxy_);
    }
    if (!token.empty())
      client->setRequestHeader("Authorization", token);

    client->setVerbose(false);

    if (client->submit() && client->getResponseCode() == 200) {
      const std::vector<char> &response_body = client->getResponseBody();
      if (!response_body.empty()) {
        std::string controller = std::string(response_body.begin(), response_body.end());
        logger_->log_trace("controller config {}", controller);
        rapidjson::Document doc;
        rapidjson::ParseResult ok = doc.Parse(controller.c_str());

        if (ok && doc.IsObject() && !doc.ObjectEmpty()) {
          rapidjson::Value::MemberIterator itr = doc.FindMember("controller");

          bool site2site_secure = false;
          if (itr != doc.MemberEnd() && itr->value.IsObject()) {
            rapidjson::Value controllerValue = itr->value.GetObject();
            rapidjson::Value::ConstMemberIterator end_itr = controllerValue.MemberEnd();
            rapidjson::Value::ConstMemberIterator port_itr = controllerValue.FindMember("remoteSiteListeningPort");
            rapidjson::Value::ConstMemberIterator secure_itr = controllerValue.FindMember("siteToSiteSecure");
            site2site_secure = secure_itr != end_itr && secure_itr->value.IsBool() ? secure_itr->value.GetBool() : false;

            if (client_type_ == sitetosite::ClientType::RAW && port_itr != end_itr && port_itr->value.IsNumber()) {
              siteTosite_port = port_itr->value.GetInt();
            } else {
              siteTosite_port = nifi_port;
            }
          }

          logger_->log_debug("process group remote site2site port {}, is secure {}", siteTosite_port, site2site_secure);
          return std::make_pair(host, siteTosite_port);
        }
      } else {
        logger_->log_error("Cannot output body to content for ProcessGroup::refreshRemoteSite2SiteInfo: received HTTP code {} from {}", client->getResponseCode(), fullUrl.str());
      }
    } else {
      logger_->log_error("ProcessGroup::refreshRemoteSite2SiteInfo -- curl_easy_perform() failed , response code {}\n", client->getResponseCode());
    }
  }
  return std::make_pair("", -1);
}

void RemoteProcessGroupPort::refreshPeerList() {
  auto connection = refreshRemoteSite2SiteInfo();
  if (connection.second == -1) {
    logger_->log_debug("No port configured");
    return;
  }

  peers_.clear();

  std::unique_ptr<sitetosite::SiteToSiteClient> protocol;
  sitetosite::SiteToSiteClientConfiguration config(protocol_uuid_, connection.first, connection.second, getInterface(), client_type_);
  protocol = initializeProtocol(config);

  if (protocol) {
    if (auto peers = protocol->getPeerList()) {
      peers_ = *peers;
    }
  }

  logger_->log_info("Have {} peers", peers_.size());

  if (!peers_.empty())
    peer_index_ = 0;
}

}  // namespace org::apache::nifi::minifi
