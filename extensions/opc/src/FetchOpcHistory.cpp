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

#include "FetchOpcHistory.h"

#include <memory>
#include <string>

#include "minifi-cpp/core/ProcessContext.h"
#include "core/ProcessSession.h"
#include "core/Resource.h"
#include "utils/ProcessorConfigUtils.h"

namespace org::apache::nifi::minifi::processors {

void FetchOpcHistory::initialize() {
  setSupportedProperties(Properties);
  setSupportedRelationships(Relationships);
}

void FetchOpcHistory::onSchedule(core::ProcessContext& context, core::ProcessSessionFactory& factory) {
  logger_->log_trace("FetchOpcHistory::onSchedule");

  BaseOPCProcessor::onSchedule(context, factory);

  node_id_ = utils::parseProperty(context, NodeID);

  parseIdType(context, NodeIDType);

  namespace_idx_ = gsl::narrow<int32_t>(utils::parseI64Property(context, NameSpaceIndex));

}

void FetchOpcHistory::onTrigger(core::ProcessContext& context, core::ProcessSession& session) {
  logger_->log_trace("FetchOpcHistory::onTrigger");

  if (!reconnect()) {
    context.yield();
    return;
  }

  auto* state_manager = context.getStateManager();
  std::unordered_map<std::string, std::string> state_map;
  state_manager->get(state_map);


  state_manager->set(state_map);
}


REGISTER_RESOURCE(FetchOpcHistory, Processor);

}  // namespace org::apache::nifi::minifi::processors
