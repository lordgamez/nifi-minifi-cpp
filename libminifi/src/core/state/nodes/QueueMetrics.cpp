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

#include "core/state/nodes/QueueMetrics.h"
#include "core/Resource.h"

namespace org::apache::nifi::minifi::state::response {

std::vector<SerializedResponseNode> QueueMetrics::serialize() {
  std::vector<SerializedResponseNode> serialized;
  for (const auto& [_, connection] : connection_store_.getConnections()) {
    SerializedResponseNode parent;
    parent.name = connection->getName();
    SerializedResponseNode datasize;
    datasize.name = "datasize";
    datasize.value = std::to_string(connection->getQueueDataSize());

    SerializedResponseNode datasizemax;
    datasizemax.name = "datasizemax";
    datasizemax.value = std::to_string(connection->getMaxQueueDataSize());

    SerializedResponseNode queuesize;
    queuesize.name = "queued";
    queuesize.value = std::to_string(connection->getQueueSize());

    SerializedResponseNode queuesizemax;
    queuesizemax.name = "queuedmax";
    queuesizemax.value = std::to_string(connection->getMaxQueueSize());

    parent.children.push_back(datasize);
    parent.children.push_back(datasizemax);
    parent.children.push_back(queuesize);
    parent.children.push_back(queuesizemax);

    serialized.push_back(parent);
  }
  return serialized;
}

REGISTER_RESOURCE(QueueMetrics, DescriptionOnly);

}  // namespace org::apache::nifi::minifi::state::response

