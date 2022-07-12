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

#include <optional>
#include <memory>
#include <system_error>

#include "utils/MinifiConcurrentQueue.h"
#include "core/logging/Logger.h"
#include "core/logging/LoggerConfiguration.h"
#include "asio/ts/buffer.hpp"
#include "asio/ts/internet.hpp"
#include "asio/streambuf.hpp"
#include "Server.h"

namespace org::apache::nifi::minifi::utils::net {

template<typename SocketReturnType, typename ReadStream>
class Session : public std::enable_shared_from_this<Session<SocketReturnType, ReadStream>> {
 public:
  Session(utils::ConcurrentQueue<Message>& concurrent_queue, std::optional<size_t> max_queue_size, std::shared_ptr<core::logging::Logger> logger)
    : concurrent_queue_(concurrent_queue),
      max_queue_size_(max_queue_size),
      logger_(std::move(logger)) {
  }

  virtual SocketReturnType& getSocket() = 0;

  void start() {
    asio::async_read_until(getReadStream(),
                           buffer_,
                           '\n',
                           [self = this->shared_from_this()](const auto& error_code, size_t) -> void {
                             self->handleReadUntilNewLine(error_code);
                           });
  }

  void handleReadUntilNewLine(std::error_code error_code) {
    if (error_code)
      return;
    std::istream is(&buffer_);
    std::string message;
    std::getline(is, message);
    if (!max_queue_size_ || max_queue_size_ > concurrent_queue_.size())
      concurrent_queue_.enqueue(Message(message, IpProtocol::TCP, getSocket().remote_endpoint().address(), getSocket().local_endpoint().port()));
    else
      logger_->log_warn("Queue is full. TCP message ignored.");
    asio::async_read_until(getReadStream(),
                           buffer_,
                           '\n',
                           [self = this->shared_from_this()](const auto& error_code, size_t) -> void {
                             self->handleReadUntilNewLine(error_code);
                           });
  }

 protected:
  virtual ReadStream& getReadStream() = 0;

  utils::ConcurrentQueue<Message>& concurrent_queue_;
  std::optional<size_t> max_queue_size_;
  asio::basic_streambuf<std::allocator<char>> buffer_;
  std::shared_ptr<core::logging::Logger> logger_;
};

}  // namespace org::apache::nifi::minifi::utils::net
