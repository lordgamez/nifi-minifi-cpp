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

#include <string>

#include "utils/StringUtils.h"

namespace org::apache::nifi::minifi::c2 {

struct FlowStatusRequest {
  std::string query_type;
  std::string identifier;
  std::vector<std::string> options;

  FlowStatusRequest(const std::string& query_string) {
    auto query_parameters = minifi::utils::string::splitAndTrimRemovingEmpty(query_string, ":");
    if (query_parameters.size() < 2) {
      throw std::invalid_argument("Invalid query string: " + query_string);
    }
    query_type = query_parameters[0];
    if (query_parameters.size() > 2) {
      identifier = query_parameters[1];
      options = minifi::utils::string::splitAndTrimRemovingEmpty(query_parameters[2], ",");
    } else {
      options = minifi::utils::string::splitAndTrimRemovingEmpty(query_parameters[1], ",");
    }
  }
};

}  // namespace org::apache::nifi::minifi::c2
