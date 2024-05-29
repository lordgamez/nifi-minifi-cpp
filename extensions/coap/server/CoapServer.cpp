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
#include "CoapServer.h"
#include <coap2/coap.h>
#include <map>
#include <functional>

namespace org::apache::nifi::minifi::coap {

std::map<coap_resource_t*, std::function<CoapResponse(CoapQuery)>> CoapServer::functions_;

CoapServer::CoapServer(std::string name, const utils::Identifier &uuid)
    : core::Connectable(std::move(name), uuid),
      server_(nullptr),
      port_(0) {
  // TODO(_): this allows this class to be instantiated via the the class loader
  // need to define this capability in the future.
}

CoapServer::CoapServer(const std::string &hostname, uint16_t port)
    : core::Connectable(hostname),
      hostname_(hostname),
      server_(nullptr),
      port_(port) {
  coap_startup();
  auto port_str = std::to_string(port_);
  server_ = create_server(hostname_.c_str(), port_str.c_str());
}

CoapServer::~CoapServer() {
  running_ = false;
  future.get();
  if (server_) {
    free_server(server_);
  }
}

void CoapServer::start() {
  running_ = true;

  future = std::async(std::launch::async, [&]() -> uint64_t {
    while (running_) {
      int res = coap_run_once(server_->ctx, 100);
      if (res < 0) {
        break;
      }
      coap_check_notify(server_->ctx);
    }
    return 0;
  });
}

void CoapServer::add_endpoint(const std::string &path, Method method, std::function<CoapResponse(CoapQuery)> functor) {
  unsigned char mthd = COAP_REQUEST_POST;
  switch (method) {
    case Method::Get:
      mthd = COAP_REQUEST_GET;
      break;
    case Method::Post:
      mthd = COAP_REQUEST_POST;
      break;
    case Method::Put:
      mthd = COAP_REQUEST_PUT;
      break;
    case Method::Delete:
      mthd = COAP_REQUEST_DELETE;
      break;
  }
  auto current_endpoint = endpoints_.find(path);
  if (current_endpoint != endpoints_.end()) {
    ::add_endpoint(current_endpoint->second, mthd, handle_response_with_passthrough);
  } else {
    CoapEndpoint * const endpoint = create_endpoint(server_, path.c_str(), mthd, handle_response_with_passthrough);
    functions_.insert(std::make_pair(endpoint->resource, functor));
    endpoints_.insert(std::make_pair(path, endpoint));
  }
}

void CoapServer::add_endpoint(Method method, std::function<CoapResponse(CoapQuery)> functor) {
  unsigned char mthd = COAP_REQUEST_POST;
  switch (method) {
    case Method::Get:
      mthd = COAP_REQUEST_GET;
      break;
    case Method::Post:
      mthd = COAP_REQUEST_POST;
      break;
    case Method::Put:
      mthd = COAP_REQUEST_PUT;
      break;
    case Method::Delete:
      mthd = COAP_REQUEST_DELETE;
      break;
  }
  CoapEndpoint * const endpoint = create_endpoint(server_, NULL, mthd, handle_response_with_passthrough);
  functions_.insert(std::make_pair(endpoint->resource, functor));
  endpoints_.insert(std::make_pair("", endpoint));
}

void CoapServer::handle_response_with_passthrough(coap_context_t* /*ctx*/,
                                                  struct coap_resource_t *resource,
                                                  coap_session_t *session,
                                                  coap_pdu_t *request,
                                                  coap_binary_t* /*token*/,
                                                  coap_string_t* /*query*/,
                                                  coap_pdu_t *response) {
  auto fx = functions_.find(resource);
  if (fx != functions_.end()) {
    auto message = create_coap_message(request);
    CoapQuery qry("", std::unique_ptr<CoapMessage, decltype(&free_coap_message)>(message, free_coap_message));
    // call the UDF
    auto udfResponse = fx->second(std::move(qry));
    response = coap_pdu_init(COAP_MESSAGE_CON, COAP_RESPONSE_CODE(udfResponse.getCode()), coap_new_message_id(session), udfResponse.getSize() + 1);
    coap_add_data(response, udfResponse.getSize(), udfResponse.getData());
    if (coap_send(session, response) == COAP_INVALID_TID) {
      printf("error while returning response");
    }
  }
}

}  // namespace org::apache::nifi::minifi::coap {
