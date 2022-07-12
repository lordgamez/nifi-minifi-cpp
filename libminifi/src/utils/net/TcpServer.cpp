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
#include "utils/net/TcpServer.h"

namespace org::apache::nifi::minifi::utils::net {

TcpSession::TcpSession(asio::io_context& io_context, utils::ConcurrentQueue<Message>& concurrent_queue, std::optional<size_t> max_queue_size, std::shared_ptr<core::logging::Logger> logger)
  : Session<asio::ip::tcp::socket, asio::ip::tcp::socket>(concurrent_queue, max_queue_size, logger),
    socket_(io_context) {
}

asio::ip::tcp::socket& TcpSession::getSocket() {
  return socket_;
}

asio::ip::tcp::socket& TcpSession::getReadStream() {
  return socket_;
}

TcpServer::TcpServer(std::optional<size_t> max_queue_size, uint16_t port, std::shared_ptr<core::logging::Logger> logger)
    : SessionHandlingServer<TcpSession>(max_queue_size, port, std::move(logger)) {
  startAccept();
}

std::shared_ptr<TcpSession> TcpServer::createSession() {
  return std::make_shared<TcpSession>(io_context_, concurrent_queue_, max_queue_size_, logger_);
}

}  // namespace org::apache::nifi::minifi::utils::net
