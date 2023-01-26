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
#include "core/state/nodes/AgentInformation.h"
#include "core/Resource.h"

namespace org::apache::nifi::minifi::state::response {

std::vector<SerializedResponseNode> ComponentManifest::serialize() {
  std::vector<SerializedResponseNode> serialized;
  SerializedResponseNode resp;
  resp.name = "componentManifest";
  struct Components group = build_description_.getClassDescriptions(getName());
  serializeClassDescription(group.processors_, "processors", resp);
  serializeClassDescription(group.controller_services_, "controllerServices", resp);
  serialized.push_back(resp);
  return serialized;
}

void ComponentManifest::serializeClassDescription(const std::vector<ClassDescription>& descriptions, const std::string& name, SerializedResponseNode& response) const {
  if (!descriptions.empty()) {
    SerializedResponseNode type;
    type.name = name;
    type.array = true;
    std::vector<SerializedResponseNode> serialized;
    for (const auto& group : descriptions) {
      SerializedResponseNode desc;
      desc.name = group.full_name_;
      SerializedResponseNode className;
      className.name = "type";
      className.value = group.full_name_;

      if (!group.class_properties_.empty()) {
        SerializedResponseNode props;
        props.name = "propertyDescriptors";
        for (auto && prop : group.class_properties_) {
          SerializedResponseNode child;
          child.name = prop.getName();

          SerializedResponseNode descriptorName;
          descriptorName.name = "name";
          descriptorName.value = prop.getName();

          SerializedResponseNode descriptorDescription;
          descriptorDescription.name = "description";
          descriptorDescription.value = prop.getDescription();

          SerializedResponseNode validatorName;
          validatorName.name = "validator";
          if (prop.getValidator()) {
            validatorName.value = prop.getValidator()->getName();
          } else {
            validatorName.value = "VALID";
          }

          SerializedResponseNode supportsExpressionLanguageScope;
          supportsExpressionLanguageScope.name = "expressionLanguageScope";
          supportsExpressionLanguageScope.value = prop.supportsExpressionLanguage() ? "FLOWFILE_ATTRIBUTES" : "NONE";

          SerializedResponseNode descriptorRequired;
          descriptorRequired.name = "required";
          descriptorRequired.value = prop.getRequired();

          SerializedResponseNode descriptorValidRegex;
          descriptorValidRegex.name = "validRegex";
          descriptorValidRegex.value = prop.getValidRegex();

          SerializedResponseNode descriptorDefaultValue;
          descriptorDefaultValue.name = "defaultValue";
          descriptorDefaultValue.value = prop.getValue();

          SerializedResponseNode descriptorDependentProperties;
          descriptorDependentProperties.name = "dependentProperties";

          for (const auto &propName : prop.getDependentProperties()) {
            SerializedResponseNode descriptorDependentProperty;
            descriptorDependentProperty.name = propName;
            descriptorDependentProperties.children.push_back(descriptorDependentProperty);
          }

          SerializedResponseNode descriptorExclusiveOfProperties;
          descriptorExclusiveOfProperties.name = "exclusiveOfProperties";

          for (const auto &exclusiveProp : prop.getExclusiveOfProperties()) {
            SerializedResponseNode descriptorExclusiveOfProperty;
            descriptorExclusiveOfProperty.name = exclusiveProp.first;
            descriptorExclusiveOfProperty.value = exclusiveProp.second;
            descriptorExclusiveOfProperties.children.push_back(descriptorExclusiveOfProperty);
          }

          const auto &allowed_types = prop.getAllowedTypes();
          if (!allowed_types.empty()) {
            SerializedResponseNode allowed_type;
            allowed_type.name = "typeProvidedByValue";
            for (const auto &type : allowed_types) {
              std::string class_name = utils::StringUtils::split(type, "::").back();
              SerializedResponseNode typeNode;
              typeNode.name = "type";
              std::string typeClazz = type;
              utils::StringUtils::replaceAll(typeClazz, "::", ".");
              typeNode.value = typeClazz;

              SerializedResponseNode bgroup;
              bgroup.name = "group";
              bgroup.value = GROUP_STR;

              SerializedResponseNode artifact;
              artifact.name = "artifact";
              artifact.value = core::ClassLoader::getDefaultClassLoader().getGroupForClass(class_name).value_or("");
              allowed_type.children.push_back(typeNode);
              allowed_type.children.push_back(bgroup);
              allowed_type.children.push_back(artifact);
            }
            child.children.push_back(allowed_type);
          }

          child.children.push_back(descriptorName);

          if (prop.getName() != prop.getDisplayName()) {
            SerializedResponseNode displayName;
            displayName.name = "displayName";
            displayName.value = prop.getDisplayName();
            child.children.push_back(displayName);
          }
          child.children.push_back(descriptorDescription);
          child.children.push_back(validatorName);
          child.children.push_back(descriptorRequired);
          child.children.push_back(supportsExpressionLanguageScope);
          child.children.push_back(descriptorDefaultValue);
          child.children.push_back(descriptorValidRegex);
          child.children.push_back(descriptorDependentProperties);
          child.children.push_back(descriptorExclusiveOfProperties);

          if (!prop.getAllowedValues().empty()) {
            SerializedResponseNode allowedValues;
            allowedValues.name = "allowableValues";
            allowedValues.array = true;
            for (const auto &av : prop.getAllowedValues()) {
              SerializedResponseNode allowableValue;
              allowableValue.name = "allowableValues";

              SerializedResponseNode allowedValue;
              allowedValue.name = "value";
              allowedValue.value = av;
              SerializedResponseNode allowedDisplayName;
              allowedDisplayName.name = "displayName";
              allowedDisplayName.value = av;

              allowableValue.children.push_back(allowedValue);
              allowableValue.children.push_back(allowedDisplayName);

              allowedValues.children.push_back(allowableValue);
            }
            child.children.push_back(allowedValues);
          }

          props.children.push_back(child);
        }

        desc.children.push_back(props);
      }

      SerializedResponseNode dyn_prop;
      dyn_prop.name = "supportsDynamicProperties";
      dyn_prop.value = group.dynamic_properties_;

      SerializedResponseNode dyn_relat;
      dyn_relat.name = "supportsDynamicRelationships";
      dyn_relat.value = group.dynamic_relationships_;

      // only for processors
      if (!group.class_relationships_.empty()) {
        SerializedResponseNode inputReq;
        inputReq.name = "inputRequirement";
        inputReq.value = group.inputRequirement_;

        SerializedResponseNode isSingleThreaded;
        isSingleThreaded.name = "isSingleThreaded";
        isSingleThreaded.value = group.isSingleThreaded_;

        SerializedResponseNode relationships;
        relationships.name = "supportedRelationships";
        relationships.array = true;

        for (const auto &relationship : group.class_relationships_) {
          SerializedResponseNode child;
          child.name = "supportedRelationships";

          SerializedResponseNode nameNode;
          nameNode.name = "name";
          nameNode.value = relationship.getName();

          SerializedResponseNode descriptorDescription;
          descriptorDescription.name = "description";
          descriptorDescription.value = relationship.getDescription();
          child.children.push_back(nameNode);
          child.children.push_back(descriptorDescription);

          relationships.children.push_back(child);
        }
        desc.children.push_back(inputReq);
        desc.children.push_back(isSingleThreaded);
        desc.children.push_back(relationships);
      }

      auto lastOfIdx = group.full_name_.find_last_of('.');
      std::string processorName = group.full_name_;
      if (lastOfIdx != std::string::npos) {
        lastOfIdx++;  // if a value is found, increment to move beyond the .
        size_t nameLength = group.full_name_.length() - lastOfIdx;
        processorName = group.full_name_.substr(lastOfIdx, nameLength);
      }


      {
        SerializedResponseNode proc_desc;
        proc_desc.name = "typeDescription";
        proc_desc.value = group.description_;
        desc.children.push_back(proc_desc);
      }

      desc.children.push_back(dyn_relat);
      desc.children.push_back(dyn_prop);
      desc.children.push_back(className);

      type.children.push_back(desc);
    }
    response.children.push_back(type);
  }
}

std::vector<SerializedResponseNode> ExternalManifest::serialize() {
  std::vector<SerializedResponseNode> serialized;
  SerializedResponseNode resp;
  resp.name = "componentManifest";
  struct Components group = ExternalBuildDescription::getClassDescriptions(getName());
  serializeClassDescription(group.processors_, "processors", resp);
  serializeClassDescription(group.controller_services_, "controllerServices", resp);
  serialized.push_back(resp);
  return serialized;
}

std::vector<SerializedResponseNode> Bundles::serialize() {
  std::vector<SerializedResponseNode> serialized;
  for (auto group : AgentBuild::getExtensions()) {
    SerializedResponseNode bundle;
    bundle.name = "bundles";

    SerializedResponseNode bgroup;
    bgroup.name = "group";
    bgroup.value = GROUP_STR;
    SerializedResponseNode artifact;
    artifact.name = "artifact";
    artifact.value = group;
    SerializedResponseNode version;
    version.name = "version";
    version.value = AgentBuild::VERSION;

    bundle.children.push_back(bgroup);
    bundle.children.push_back(artifact);
    bundle.children.push_back(version);

    ComponentManifest compMan(group);
    // serialize the component information.
    for (auto component : compMan.serialize()) {
      bundle.children.push_back(component);
    }
    serialized.push_back(bundle);
  }

  // let's provide our external manifests.
  for (auto group : ExternalBuildDescription::getExternalGroups()) {
    SerializedResponseNode bundle;
    bundle.name = "bundles";

    SerializedResponseNode bgroup;
    bgroup.name = "group";
    bgroup.value = group.group;
    SerializedResponseNode artifact;
    artifact.name = "artifact";
    artifact.value = group.artifact;
    SerializedResponseNode version;
    version.name = "version";
    version.value = group.version;

    bundle.children.push_back(bgroup);
    bundle.children.push_back(artifact);
    bundle.children.push_back(version);

    ExternalManifest compMan(group.artifact);
    // serialize the component information.
    for (auto component : compMan.serialize()) {
      bundle.children.push_back(component);
    }
    serialized.push_back(bundle);
  }

  return serialized;
}

std::vector<SerializedResponseNode> AgentStatus::serialize() {
  std::vector<SerializedResponseNode> serialized;
  auto serializedRepositories = serializeRepositories();
  if (!serializedRepositories.empty()) {
    serialized.push_back(serializedRepositories);
  }
  serialized.push_back(serializeUptime());

  auto serializedComponents = serializeComponents();
  if (!serializedComponents.empty()) {
    serialized.push_back(serializedComponents);
  }

  serialized.push_back(serializeResourceConsumption());

  return serialized;
}

std::vector<PublishedMetric> AgentStatus::calculateMetrics() {
  std::vector<PublishedMetric> metrics;
  for (const auto& [_, repo] : repositories_) {
    metrics.push_back({"is_running", (repo->isRunning() ? 1.0 : 0.0), {{"metric_class", getName()}, {"repository_name", repo->getName()}}});
    metrics.push_back({"is_full", (repo->isFull() ? 1.0 : 0.0), {{"metric_class", getName()}, {"repository_name", repo->getName()}}});
    metrics.push_back({"repository_size", static_cast<double>(repo->getRepoSize()), {{"metric_class", getName()}, {"repository_name", repo->getName()}}});
  }
  if (nullptr != monitor_) {
    auto uptime = monitor_->getUptime();
    metrics.push_back({"uptime_milliseconds", static_cast<double>(uptime), {{"metric_class", getName()}}});
  }

  if (nullptr != monitor_) {
    monitor_->executeOnAllComponents([this, &metrics](StateController& component){
      metrics.push_back({"is_running", (component.isRunning() ? 1.0 : 0.0),
        {{"component_uuid", component.getComponentUUID().to_string()}, {"component_name", component.getComponentName()}, {"metric_class", getName()}}});
    });
  }

  metrics.push_back({"agent_memory_usage_bytes", static_cast<double>(utils::OsUtils::getCurrentProcessPhysicalMemoryUsage()), {{"metric_class", getName()}}});

  double cpu_usage = -1.0;
  {
    std::lock_guard<std::mutex> guard(cpu_load_tracker_mutex_);
    cpu_usage = cpu_load_tracker_.getCpuUsageAndRestartCollection();
  }
  metrics.push_back({"agent_cpu_utilization", cpu_usage, {{"metric_class", getName()}}});
  return metrics;
}

SerializedResponseNode AgentStatus::serializeRepositories() const {
  SerializedResponseNode repositories;

  repositories.name = "repositories";

  for (const auto& repo : repositories_) {
    SerializedResponseNode repoNode;
    repoNode.collapsible = false;
    repoNode.name = repo.first;

    SerializedResponseNode queuesize;
    queuesize.name = "size";
    queuesize.value = repo.second->getRepoSize();

    SerializedResponseNode isRunning;
    isRunning.name = "running";
    isRunning.value = repo.second->isRunning();

    SerializedResponseNode isFull;
    isFull.name = "full";
    isFull.value = repo.second->isFull();

    repoNode.children.push_back(queuesize);
    repoNode.children.push_back(isRunning);
    repoNode.children.push_back(isFull);
    repositories.children.push_back(repoNode);
  }
  return repositories;
}

SerializedResponseNode AgentStatus::serializeUptime() const {
  SerializedResponseNode uptime;

  uptime.name = "uptime";
  if (nullptr != monitor_) {
    uptime.value = monitor_->getUptime();
  } else {
    uptime.value = "0";
  }

  return uptime;
}

SerializedResponseNode AgentStatus::serializeComponents() const {
  SerializedResponseNode components_node;
  components_node.collapsible = false;
  components_node.name = "components";
  if (monitor_ != nullptr) {
    monitor_->executeOnAllComponents([&components_node](StateController& component){
      SerializedResponseNode component_node;
      component_node.collapsible = false;
      component_node.name = component.getComponentName();

      SerializedResponseNode uuid_node;
      uuid_node.name = "uuid";
      uuid_node.value = std::string{component.getComponentUUID().to_string()};

      SerializedResponseNode component_status_node;
      component_status_node.name = "running";
      component_status_node.value = component.isRunning();

      component_node.children.push_back(component_status_node);
      component_node.children.push_back(uuid_node);
      components_node.children.push_back(component_node);
    });
  }
  return components_node;
}

SerializedResponseNode AgentStatus::serializeAgentMemoryUsage() const {
  SerializedResponseNode used_physical_memory;
  used_physical_memory.name = "memoryUsage";
  used_physical_memory.value = utils::OsUtils::getCurrentProcessPhysicalMemoryUsage();
  return used_physical_memory;
}

SerializedResponseNode AgentStatus::serializeAgentCPUUsage() const {
  double system_cpu_usage = -1.0;
  {
    std::lock_guard<std::mutex> guard(cpu_load_tracker_mutex_);
    system_cpu_usage = cpu_load_tracker_.getCpuUsageAndRestartCollection();
  }
  SerializedResponseNode cpu_usage;
  cpu_usage.name = "cpuUtilization";
  cpu_usage.value = system_cpu_usage;
  return cpu_usage;
}

SerializedResponseNode AgentStatus::serializeResourceConsumption() const {
  SerializedResponseNode resource_consumption;
  resource_consumption.name = "resourceConsumption";
  resource_consumption.children.push_back(serializeAgentMemoryUsage());
  resource_consumption.children.push_back(serializeAgentCPUUsage());
  return resource_consumption;
}

std::vector<SerializedResponseNode> AgentManifest::serialize() {
  std::vector<SerializedResponseNode> serialized = {
      {.name = "identifier", .value = AgentBuild::BUILD_IDENTIFIER},
      {.name = "agentType", .value = "cpp"},
      {.name = "buildInfo", .children = {
          {.name = "flags", .value = AgentBuild::COMPILER_FLAGS},
          {.name = "compiler", .value = AgentBuild::COMPILER},
          {.name = "version", .value = AgentBuild::VERSION},
          {.name = "revision", .value = AgentBuild::BUILD_REV},
          {.name = "timestamp", .value = static_cast<uint64_t>(std::stoull(AgentBuild::BUILD_DATE))}
      }}
  };
  {
    auto bundles = Bundles{"bundles"}.serialize();
    std::move(std::begin(bundles), std::end(bundles), std::back_inserter(serialized));
  }
  {
    auto schedulingDefaults = SchedulingDefaults{"schedulingDefaults"}.serialize();
    std::move(std::begin(schedulingDefaults), std::end(schedulingDefaults), std::back_inserter(serialized));
  }
  {
    auto supportedOperations = [this]() {
      SupportedOperations supported_operations("supportedOperations");
      supported_operations.setStateMonitor(monitor_);
      supported_operations.setUpdatePolicyController(update_policy_controller_);
      supported_operations.setConfigurationReader(configuration_reader_);
      return supported_operations.serialize();
    }();
    std::move(std::begin(supportedOperations), std::end(supportedOperations), std::back_inserter(serialized));
  }
  return serialized;
}

std::vector<SerializedResponseNode> AgentNode::serialize() {
  std::vector<SerializedResponseNode> serialized = {
      {.name = "identifier", .value = provider_->getAgentIdentifier()},
  };

  const auto agent_class = provider_->getAgentClass();
  if (agent_class) {
    serialized.push_back({.name = "agentClass", .value = *agent_class});
  }

  serialized.push_back({.name = "agentManifestHash", .value = getAgentManifestHash()});
  return serialized;
}

std::vector<SerializedResponseNode> AgentNode::getAgentManifest() const {
  if (agent_manifest_cache_) { return std::vector{*agent_manifest_cache_}; }
  agent_manifest_cache_ = {.name = "agentManifest", .children = [this] {
    AgentManifest manifest{"manifest"};
    manifest.setStateMonitor(monitor_);
    manifest.setUpdatePolicyController(update_policy_controller_);
    manifest.setConfigurationReader(configuration_reader_);
    return manifest.serialize();
  }()};
  agent_manifest_hash_cache_.clear();
  return std::vector{ *agent_manifest_cache_ };
}

std::string AgentNode::getAgentManifestHash() const {
  if (agent_manifest_hash_cache_.empty()) {
    agent_manifest_hash_cache_ = hashResponseNodes(getAgentManifest());
  }
  return agent_manifest_hash_cache_;
}

std::vector<SerializedResponseNode> AgentNode::getAgentStatus() const {
  std::vector<SerializedResponseNode> serialized;

  AgentStatus status("status");
  status.setRepositories(repositories_);
  status.setStateMonitor(monitor_);

  SerializedResponseNode agentStatus;
  agentStatus.name = "status";
  for (auto &ser : status.serialize()) {
    agentStatus.children.push_back(std::move(ser));
  }

  serialized.push_back(agentStatus);
  return serialized;
}

std::vector<SerializedResponseNode> AgentInformation::serialize() {
  std::vector<SerializedResponseNode> serialized(AgentNode::serialize());
  if (include_agent_manifest_) {
    auto manifest = getAgentManifest();
    serialized.insert(serialized.end(), std::make_move_iterator(manifest.begin()), std::make_move_iterator(manifest.end()));
  }

  if (include_agent_status_) {
    auto status = getAgentStatus();
    serialized.insert(serialized.end(), std::make_move_iterator(status.begin()), std::make_move_iterator(status.end()));
  }
  return serialized;
}

utils::ProcessCpuUsageTracker AgentStatus::cpu_load_tracker_;
std::mutex AgentStatus::cpu_load_tracker_mutex_;

REGISTER_RESOURCE(AgentInformation, DescriptionOnly);
REGISTER_RESOURCE(AgentStatus, DescriptionOnly);

}  // namespace org::apache::nifi::minifi::state::response
