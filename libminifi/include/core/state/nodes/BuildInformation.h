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
#include <utility>
#include <vector>

#ifndef WIN32

#if ( defined(__APPLE__) || defined(__MACH__) || defined(BSD))
#include <net/if_dl.h>
#include <net/if_types.h>
#endif

#include <arpa/inet.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <ifaddrs.h>
#include <netinet/in.h>
#include <net/if.h>
#include <netdb.h>
#include <unistd.h>

#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <functional>
#include <map>
#include <sstream>

#include "../../../agent/agent_version.h"
#include "../nodes/MetricsBase.h"
#include "Connection.h"
#include "core/ClassLoader.h"
#include "io/ClientSocket.h"

namespace org::apache::nifi::minifi::state::response {

/**
 * Justification and Purpose: Provides build information
 * for this agent.
 */
class BuildInformation : public DeviceInformation {
 public:
  BuildInformation(std::string name, const utils::Identifier &uuid)
      : DeviceInformation(std::move(name), uuid) {
  }

  explicit BuildInformation(std::string name)
      : DeviceInformation(std::move(name)) {
  }

  MINIFIAPI static constexpr const char* Description = "Metric node that defines the pertinent build information for this agent binary";

  std::string getName() const override {
    return "BuildInformation";
  }

  std::vector<SerializedResponseNode> serialize() override;
};

}  // namespace org::apache::nifi::minifi::state::response
