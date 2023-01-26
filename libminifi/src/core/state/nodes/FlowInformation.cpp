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

#include "core/state/nodes/FlowInformation.h"
#include "core/Resource.h"

namespace org::apache::nifi::minifi::state::response {


std::vector<SerializedResponseNode> FlowVersion::serialize() {
  std::lock_guard<std::mutex> lock(guard);
  std::vector<SerializedResponseNode> serialized;
  SerializedResponseNode ru;
  ru.name = "registryUrl";
  ru.value = identifier->getRegistryUrl();

  SerializedResponseNode bucketid;
  bucketid.name = "bucketId";
  bucketid.value = identifier->getBucketId();

  SerializedResponseNode flowId;
  flowId.name = "flowId";
  flowId.value = identifier->getFlowId();

  serialized.push_back(ru);
  serialized.push_back(bucketid);
  serialized.push_back(flowId);
  return serialized;
}


std::vector<SerializedResponseNode> FlowInformation::serialize() {
  std::vector<SerializedResponseNode> serialized;

  SerializedResponseNode fv;
  fv.name = "flowId";
  fv.value = flow_version_->getFlowId();

  SerializedResponseNode uri;
  uri.name = "versionedFlowSnapshotURI";
  for (auto &entry : flow_version_->serialize()) {
    uri.children.push_back(entry);
  }

  serialized.push_back(fv);
  serialized.push_back(uri);

  const auto& connections = connection_store_.getConnections();
  if (!connections.empty()) {
    SerializedResponseNode queues;
    queues.collapsible = false;
    queues.name = "queues";

    for (const auto& queue : connections) {
      SerializedResponseNode repoNode;
      repoNode.collapsible = false;
      repoNode.name = queue.second->getName();

      SerializedResponseNode queueUUIDNode;
      queueUUIDNode.name = "uuid";
      queueUUIDNode.value = std::string{queue.second->getUUIDStr()};

      SerializedResponseNode queuesize;
      queuesize.name = "size";
      queuesize.value = queue.second->getQueueSize();

      SerializedResponseNode queuesizemax;
      queuesizemax.name = "sizeMax";
      queuesizemax.value = queue.second->getMaxQueueSize();

      SerializedResponseNode datasize;
      datasize.name = "dataSize";
      datasize.value = queue.second->getQueueDataSize();
      SerializedResponseNode datasizemax;

      datasizemax.name = "dataSizeMax";
      datasizemax.value = queue.second->getMaxQueueDataSize();

      repoNode.children.push_back(queuesize);
      repoNode.children.push_back(queuesizemax);
      repoNode.children.push_back(datasize);
      repoNode.children.push_back(datasizemax);
      repoNode.children.push_back(queueUUIDNode);

      queues.children.push_back(repoNode);
    }
    serialized.push_back(queues);
  }

  if (nullptr != monitor_) {
    SerializedResponseNode componentsNode;
    componentsNode.collapsible = false;
    componentsNode.name = "components";

    monitor_->executeOnAllComponents([&componentsNode](StateController& component){
      SerializedResponseNode componentNode;
      componentNode.collapsible = false;
      componentNode.name = component.getComponentName();

      SerializedResponseNode uuidNode;
      uuidNode.name = "uuid";
      uuidNode.value = std::string{component.getComponentUUID().to_string()};

      SerializedResponseNode componentStatusNode;
      componentStatusNode.name = "running";
      componentStatusNode.value = component.isRunning();

      componentNode.children.push_back(componentStatusNode);
      componentNode.children.push_back(uuidNode);
      componentsNode.children.push_back(componentNode);
    });
    serialized.push_back(componentsNode);
  }

  return serialized;
}

std::vector<PublishedMetric> FlowInformation::calculateMetrics() {
  std::vector<PublishedMetric> metrics = connection_store_.calculateConnectionMetrics("FlowInformation");

  if (nullptr != monitor_) {
    monitor_->executeOnAllComponents([&metrics](StateController& component){
      metrics.push_back({"is_running", (component.isRunning() ? 1.0 : 0.0),
        {{"component_uuid", component.getComponentUUID().to_string()}, {"component_name", component.getComponentName()}, {"metric_class", "FlowInformation"}}});
    });
  }
  return metrics;
}

REGISTER_RESOURCE(FlowInformation, DescriptionOnly);

}  // namespace org::apache::nifi::minifi::state::response
