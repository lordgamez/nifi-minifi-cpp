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

#include <memory>
#include <string>
#include <utility>
#include <vector>

#ifndef WIN32
#include <arpa/inet.h>
#include <sys/ioctl.h>

#if ( defined(__APPLE__) || defined(__MACH__) || defined(BSD))
#include <net/if_dl.h>
#include <net/if_types.h>

#endif
#include <ifaddrs.h>
#include <net/if.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <functional>
#include <map>
#include <sstream>

#include "agent/agent_docs.h"
#include "agent/agent_version.h"
#include "agent/build_description.h"
#include "Connection.h"
#include "core/ClassLoader.h"
#include "core/ProcessorConfig.h"
#include "core/state/nodes/MetricsBase.h"
#include "core/state/nodes/StateMonitor.h"
#include "io/ClientSocket.h"
#include "SchedulingNodes.h"
#include "utils/OsUtils.h"
#include "utils/ProcessCpuUsageTracker.h"
#include "core/AgentIdentificationProvider.h"
#include "utils/Export.h"
#include "SupportedOperations.h"

namespace org::apache::nifi::minifi::state::response {

#define GROUP_STR "org.apache.nifi.minifi"

class ComponentManifest : public DeviceInformation {
 public:
  ComponentManifest(std::string name, const utils::Identifier& uuid)
    : DeviceInformation(std::move(name), uuid) {
  }

  explicit ComponentManifest(std::string name)
    : DeviceInformation(std::move(name)) {
  }

  std::string getName() const override {
    return CoreComponent::getName();
  }

  std::vector<SerializedResponseNode> serialize() override;

 protected:
  void serializeClassDescription(const std::vector<ClassDescription>& descriptions, const std::string& name, SerializedResponseNode& response) const;

 private:
  BuildDescription build_description_;
};

class ExternalManifest : public ComponentManifest {
 public:
  ExternalManifest(std::string name, const utils::Identifier& uuid)
    : ComponentManifest(std::move(name), uuid) {
  }

  explicit ExternalManifest(std::string name)
    : ComponentManifest(std::move(name)) {
  }

  std::vector<SerializedResponseNode> serialize() override;
};

class Bundles : public DeviceInformation {
 public:
  Bundles(std::string name, const utils::Identifier& uuid)
      : DeviceInformation(std::move(name), uuid) {
    setArray(true);
  }

  explicit Bundles(std::string name)
      : DeviceInformation(std::move(name)) {
    setArray(true);
  }

  std::string getName() const override {
    return "bundles";
  }

  std::vector<SerializedResponseNode> serialize() override;
};

/**
 * Justification and Purpose: Provides available extensions for the agent information block.
 */
class AgentStatus : public StateMonitorNode {
 public:
  AgentStatus(std::string name, const utils::Identifier& uuid)
    : StateMonitorNode(std::move(name), uuid) {
  }

  explicit AgentStatus(std::string name)
    : StateMonitorNode(std::move(name)) {
  }

  MINIFIAPI static constexpr const char* Description = "Metric node that defines current agent status including repository, component and resource usage information.";

  std::string getName() const override {
    return "AgentStatus";
  }

  void setRepositories(const std::map<std::string, std::shared_ptr<core::Repository>> &repositories) {
    repositories_ = repositories;
  }

  void addRepository(const std::shared_ptr<core::Repository> &repo) {
    if (nullptr != repo) {
      repositories_.insert(std::make_pair(repo->getName(), repo));
    }
  }

  std::vector<SerializedResponseNode> serialize() override;
  std::vector<PublishedMetric> calculateMetrics() override;

 protected:
  SerializedResponseNode serializeRepositories() const;
  SerializedResponseNode serializeUptime() const;
  SerializedResponseNode serializeComponents() const;
  SerializedResponseNode serializeAgentMemoryUsage() const;
  SerializedResponseNode serializeAgentCPUUsage() const;
  SerializedResponseNode serializeResourceConsumption() const;

  std::map<std::string, std::shared_ptr<core::Repository>> repositories_;

  MINIFIAPI static utils::ProcessCpuUsageTracker cpu_load_tracker_;
  MINIFIAPI static std::mutex cpu_load_tracker_mutex_;
};

class AgentIdentifier {
 public:
  void setAgentIdentificationProvider(std::shared_ptr<core::AgentIdentificationProvider> provider) {
    provider_ = std::move(provider);
  }

  void includeAgentManifest(bool include) {
    include_agent_manifest_ = include;
  }

 protected:
  std::shared_ptr<core::AgentIdentificationProvider> provider_;
  bool include_agent_manifest_{true};
};

class AgentMonitor {
 public:
  void addRepository(const std::shared_ptr<core::Repository> &repo) {
    if (nullptr != repo) {
      repositories_.insert(std::make_pair(repo->getName(), repo));
    }
  }

  void setStateMonitor(state::StateMonitor* monitor) {
    monitor_ = monitor;
  }

 protected:
  std::map<std::string, std::shared_ptr<core::Repository>> repositories_;
  state::StateMonitor* monitor_ = nullptr;
};

/**
 * Justification and Purpose: Provides available extensions for the agent information block.
 */
class AgentManifest : public DeviceInformation {
 public:
  AgentManifest(std::string name, const utils::Identifier& uuid)
    : DeviceInformation(std::move(name), uuid) {
  }

  explicit AgentManifest(std::string name)
    : DeviceInformation(std::move(name)) {
  }

  std::string getName() const override {
    return "agentManifest";
  }

  void setStateMonitor(state::StateMonitor* monitor) {
    monitor_ = monitor;
  }

  void setUpdatePolicyController(controllers::UpdatePolicyControllerService* update_policy_controller) {
    update_policy_controller_ = update_policy_controller;
  }

  void setConfigurationReader(std::function<std::optional<std::string>(const std::string&)> configuration_reader) {
    configuration_reader_ = std::move(configuration_reader);
  }

  std::vector<SerializedResponseNode> serialize() override;

 private:
  state::StateMonitor* monitor_ = nullptr;
  controllers::UpdatePolicyControllerService* update_policy_controller_ = nullptr;
  std::function<std::optional<std::string>(const std::string&)> configuration_reader_;
};

class AgentNode : public DeviceInformation, public AgentMonitor, public AgentIdentifier {
 public:
  AgentNode(std::string name, const utils::Identifier& uuid)
      : DeviceInformation(std::move(name), uuid) {
    setArray(false);
  }

  explicit AgentNode(std::string name)
      : DeviceInformation(std::move(name)) {
    setArray(false);
  }

  void setUpdatePolicyController(controllers::UpdatePolicyControllerService* update_policy_controller) {
    update_policy_controller_ = update_policy_controller;
  }

  void setConfigurationReader(std::function<std::optional<std::string>(const std::string&)> configuration_reader) {
    configuration_reader_ = std::move(configuration_reader);
  }

 protected:
  std::vector<SerializedResponseNode> serialize() override;
  std::vector<SerializedResponseNode> getAgentManifest() const;
  std::string getAgentManifestHash() const;
  std::vector<SerializedResponseNode> getAgentStatus() const;

 private:
  mutable std::optional<SerializedResponseNode> agent_manifest_cache_;
  mutable std::string agent_manifest_hash_cache_;
  controllers::UpdatePolicyControllerService* update_policy_controller_ = nullptr;
  std::function<std::optional<std::string>(const std::string&)> configuration_reader_;
  std::shared_ptr<core::logging::Logger> logger_ = core::logging::LoggerFactory<AgentNode>::getLogger();
};

/**
 * This class is used for sending agent information while including
 * or excluding the agent manifest. agent status and agent manifest
 * is included by default
 */
class AgentInformation : public AgentNode {
 public:
  AgentInformation(std::string name, const utils::Identifier& uuid)
      : AgentNode(std::move(name), uuid) {
    setArray(false);
  }

  explicit AgentInformation(std::string name)
      : AgentNode(std::move(name)) {
    setArray(false);
  }

  MINIFIAPI static constexpr const char* Description = "Metric node that defines all agent information, to include the manifest, and bundle information as part of a healthy hearbeat.";

  std::string getName() const override {
    return "agentInfo";
  }

  void includeAgentStatus(bool include) {
    include_agent_status_ = include;
  }

  std::vector<SerializedResponseNode> serialize() override;

 protected:
  bool include_agent_status_{true};
};

}  // namespace org::apache::nifi::minifi::state::response
