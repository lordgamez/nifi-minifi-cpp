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

#include "core/state/nodes/BuildInformation.h"
#include "core/Resource.h"

namespace org::apache::nifi::minifi::state::response {

std::vector<SerializedResponseNode> BuildInformation::serialize() {
    std::vector<SerializedResponseNode> serialized;

    SerializedResponseNode build_version;
    build_version.name = "build_version";
    build_version.value = AgentBuild::VERSION;

    SerializedResponseNode build_rev;
    build_rev.name = "build_rev";
    build_rev.value = AgentBuild::BUILD_REV;

    SerializedResponseNode build_date;
    build_date.name = "build_date";
    build_date.value = AgentBuild::BUILD_DATE;

    SerializedResponseNode compiler;
    compiler.name = "compiler";
    {
      SerializedResponseNode compiler_command;
      compiler_command.name = "compiler_command";
      compiler_command.value = AgentBuild::COMPILER;

      SerializedResponseNode compiler_version;
      compiler_version.name = "compiler_version";
      compiler_version.value = AgentBuild::COMPILER_VERSION;

      SerializedResponseNode compiler_flags;
      compiler_flags.name = "compiler_flags";
      compiler_flags.value = AgentBuild::COMPILER_FLAGS;

      compiler.children.push_back(compiler_command);
      compiler.children.push_back(compiler_version);
      compiler.children.push_back(compiler_flags);
    }
    SerializedResponseNode device_id;
    device_id.name = "device_id";
    device_id.value = AgentBuild::BUILD_IDENTIFIER;

    serialized.push_back(build_version);
    serialized.push_back(build_rev);
    serialized.push_back(build_date);
    serialized.push_back(compiler);
    serialized.push_back(device_id);

    return serialized;
  }

REGISTER_RESOURCE(BuildInformation, DescriptionOnly);

}  // namespace org::apache::nifi::minifi::state::response
