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
#include "utils/net/SslServer.h"

namespace org::apache::nifi::minifi::utils::net {

SslSession::SslSession(asio::io_context& io_context, asio::ssl::context& context, utils::ConcurrentQueue<Message>& concurrent_queue, std::optional<size_t> max_queue_size, std::shared_ptr<core::logging::Logger> logger)
  : Session<ssl_socket::lowest_layer_type, ssl_socket>(concurrent_queue, max_queue_size, logger),
    socket_(io_context, context) {
}

ssl_socket::lowest_layer_type& SslSession::getSocket() {
  return socket_.lowest_layer();
}

ssl_socket& SslSession::getReadStream() {
  return socket_;
}

SslServer::SslServer(std::optional<size_t> max_queue_size, uint16_t port, std::shared_ptr<core::logging::Logger> logger)
    : SessionHandlingServer<SslSession>(max_queue_size, port, std::move(logger)),
      context_(asio::ssl::context::sslv23) {
  startAccept();
}

std::shared_ptr<SslSession> SslServer::createSession() {
  return std::make_shared<SslSession>(io_context_, context_, concurrent_queue_, max_queue_size_, logger_);
}

}  // namespace org::apache::nifi::minifi::utils::net
