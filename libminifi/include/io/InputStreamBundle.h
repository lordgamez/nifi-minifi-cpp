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

#include <span>
#include <memory>
#include <vector>

#include "InputStream.h"
#include "utils/gsl.h"

namespace org::apache::nifi::minifi::io {

class InputStreamBundle : public InputStream {
 public:
  [[nodiscard]] size_t size() const override;
  size_t read(std::span<std::byte> out_buffer) override;
  void addStream(std::unique_ptr<InputStream> stream);
  bool empty() const {
    return streams_.empty();
  }

 private:
  size_t stream_offset_ = 0;
  size_t stream_index_ = 0;
  std::vector<gsl::not_null<std::unique_ptr<InputStream>>> streams_;
};

}  // namespace org::apache::nifi::minifi::io
