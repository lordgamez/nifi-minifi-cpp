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

#include "c2/FlowStatusBuilder.h"

#include "utils/expected.h"
#include "utils/Id.h"

namespace org::apache::nifi::minifi::c2 {

void FlowStatusBuilder::setRoot(core::ProcessGroup* root) {
  std::lock_guard<std::mutex> guard(root_mutex_);
  root_ = root;
}

void FlowStatusBuilder::setBulletinStore(core::BulletinStore* bulletin_store) {
  bulletin_store_ = bulletin_store;
}

void FlowStatusBuilder::addProcessorStatus(core::Processor* processor, rapidjson::Value& processor_status_list, rapidjson::Document::AllocatorType& allocator,
    const std::unordered_set<std::string>& options) {
  if (!processor) {
    return;
  }
  rapidjson::Value processor_status(rapidjson::kObjectType);
  processor_status.AddMember("id", rapidjson::Value(processor->getUUIDStr().c_str(), allocator), allocator);
  processor_status.AddMember("name", rapidjson::Value(processor->getName().c_str(), allocator), allocator);

  std::vector<core::Bulletin> bulletins;
  if (bulletin_store_) {
    bulletins = bulletin_store_->getBulletinsForProcessor(processor->getUUIDStr());
  }

  if (options.contains("health")) {
    processor_status.AddMember("processorHealth", rapidjson::Value(rapidjson::kObjectType), allocator);
    processor_status["processorHealth"].AddMember("runStatus", processor->isRunning() ? rapidjson::Value("Running") : rapidjson::Value("Stopped"), allocator);
    processor_status["processorHealth"].AddMember("hasBulletins", bulletins.empty() ? rapidjson::Value(false) : rapidjson::Value(true), allocator);
  } else {
    processor_status.AddMember("processorHealth", rapidjson::Value(rapidjson::kNullType), allocator);
  }

  if (options.contains("stats")) {
    processor_status.AddMember("processorStats", rapidjson::Value(rapidjson::kObjectType), allocator);
    auto metrics = processor->getMetrics();
    processor_status["processorStats"].AddMember("flowfilesReceived", metrics->incomingFlowFiles().load(), allocator);
    processor_status["processorStats"].AddMember("flowfilesSent", metrics->transferredFlowFiles().load(), allocator);
    processor_status["processorStats"].AddMember("bytesRead", metrics->bytesRead().load(), allocator);
    processor_status["processorStats"].AddMember("bytesWritten", metrics->bytesWritten().load(), allocator);
    processor_status["processorStats"].AddMember("incomingBytes", metrics->incomingBytes().load(), allocator);
    processor_status["processorStats"].AddMember("transferredBytes", metrics->transferredBytes().load(), allocator);
    processor_status["processorStats"].AddMember("invocations", metrics->invocations().load(), allocator);
    processor_status["processorStats"].AddMember("processingNanos", metrics->processingNanos().load(), allocator);
  } else {
    processor_status.AddMember("processorStats", rapidjson::Value(rapidjson::kNullType), allocator);
  }

  if (options.contains("bulletins")) {
    processor_status.AddMember("bulletinList", rapidjson::Value(rapidjson::kArrayType), allocator);
    for (const auto& bulletin : bulletins) {
      rapidjson::Value bulletin_node(rapidjson::kObjectType);
      bulletin_node.AddMember("timestamp", std::chrono::duration_cast<std::chrono::seconds>(bulletin.timestamp.time_since_epoch()).count(), allocator);
      bulletin_node.AddMember("message", bulletin.message, allocator);
      processor_status["bulletinList"].PushBack(bulletin_node, allocator);
    }
  } else {
    processor_status.AddMember("bulletinList", rapidjson::Value(rapidjson::kNullType), allocator);
  }
  processor_status_list.PushBack(processor_status, allocator);
}

nonstd::expected<void, std::string> FlowStatusBuilder::addProcessorStatuses(rapidjson::Value& processor_status_list, rapidjson::Document::AllocatorType& allocator,
  const std::string& identifier, const std::unordered_set<std::string>& options) {
  std::lock_guard<std::mutex> guard(root_mutex_);
  if (!root_) {
    logger_->log_error("Root process group is not set for flow status builder!");
    return {};
  }

  std::vector<core::Processor*> processors;
  if (identifier.empty()) {
    logger_->log_error("Unable to get processorStatus: Query is incomplete");
    return nonstd::unexpected("Unable to get processorStatus: Query is incomplete");
  } else if (identifier == "all") {
    root_->getAllProcessors(processors);
  } else {
    core::Processor* processor = nullptr;
    processor = root_->findProcessorByName(identifier);
    if (!processor) {
      auto id_opt = minifi::utils::Identifier::parse(identifier);
      if (!id_opt) {
        logger_->log_error("Unable to get processorStatus: No processor with key '{}' to report status on", identifier);
        return nonstd::unexpected(fmt::format("Unable to get processorStatus: No processor with key '{}' to report status on", identifier));
      }
      processor = root_->findProcessorById(id_opt.value());
      if (!processor) {
        logger_->log_error("Unable to get processorStatus: No processor with key '{}' to report status on", identifier);
        return nonstd::unexpected(fmt::format("Unable to get processorStatus: No processor with key '{}' to report status on", identifier));
      }
    }
    processors.push_back(processor);
  }

  for (auto processor : processors) {
    addProcessorStatus(processor, processor_status_list, allocator, options);
  }

  return {};
}

rapidjson::Document FlowStatusBuilder::buildFlowStatus(const std::vector<FlowStatusRequest>& requests) {
  rapidjson::Document doc;
  doc.SetObject();

  auto allocator = doc.GetAllocator();

  auto handleError = [&doc, &allocator](const nonstd::expected<void, std::string>& result) {
    if (result) {
      return;
    }
    doc["errorsGeneratingReport"].GetArray().PushBack(rapidjson::Value(result.error().c_str(), allocator), allocator);
  };

  doc.AddMember("controllerServiceStatusList", rapidjson::Value(rapidjson::kNullType), allocator);
  doc.AddMember("connectionStatusList", rapidjson::Value(rapidjson::kNullType), allocator);
  doc.AddMember("remoteProcessGroupStatusList", rapidjson::Value(rapidjson::kNullType), allocator);
  doc.AddMember("instanceStatus", rapidjson::Value(rapidjson::kNullType), allocator);
  doc.AddMember("systemDiagnosticsStatus", rapidjson::Value(rapidjson::kNullType), allocator);
  doc.AddMember("processorStatusList", rapidjson::Value(rapidjson::kArrayType), allocator);
  doc.AddMember("errorsGeneratingReport", rapidjson::Value(rapidjson::kArrayType), allocator);

  for (const auto& request : requests) {
    if (request.query_type == FlowStatusQueryType::processor) {
      handleError(addProcessorStatuses(doc["processorStatusList"], allocator, request.identifier, request.options));
    }
  }

  return doc;
}

}  // namespace org::apache::nifi::minifi::c2
