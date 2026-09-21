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

#include "FetchOPCEvents.h"

#include <chrono>
#include <memory>
#include <string>
#include <vector>

#include "OPCCommon.h"
#include "core/ProcessSession.h"
#include "core/Resource.h"
#include "minifi-cpp/core/ProcessContext.h"
#include "utils/Enum.h"
#include "utils/ProcessorConfigUtils.h"
#include "utils/StringUtils.h"

using namespace std::literals::chrono_literals;

namespace org::apache::nifi::minifi::processors {

namespace {
// How long one iteration of the event thread waits for notifications to arrive from the server.
constexpr UA_UInt32 NOTIFICATION_WAIT_TIME_MS = 200;
constexpr std::chrono::milliseconds RETRY_INTERVAL = 1s;
}  // namespace

void FetchOPCEvents::initialize() {
  setSupportedProperties(Properties);
  setSupportedRelationships(Relationships);
}

void FetchOPCEvents::onSchedule(core::ProcessContext& context, core::ProcessSessionFactory& factory) {
  logger_->log_trace("FetchOPCEvents::onSchedule");
  BaseOPCProcessor::onSchedule(context, factory);

  node_id_ = utils::parseProperty(context, NodeID);
  id_type_ = utils::parseEnumProperty<opc::OPCNodeIDType>(context, NodeIDType);
  namespace_idx_ = gsl::narrow<UA_UInt16>(utils::parseU64Property(context, NameSpaceIndex));
  parseNode(context);

  const auto record_set_writer_name = utils::parseProperty(context, RecordSetWriter);
  auto controller_service = context.getControllerService(record_set_writer_name, getUUID());
  if (!record_set_writer_name.empty() && !controller_service) {
    throw Exception(PROCESS_SCHEDULE_EXCEPTION, fmt::format("Controller service '{}' not found", record_set_writer_name));
  }
  record_set_writer_ = std::dynamic_pointer_cast<core::RecordSetWriter>(controller_service);

  const auto max_queue_size = utils::parseOptionalU64Property(context, MaxQueueSize);
  subscription_options_.max_queue_size = max_queue_size && *max_queue_size > 0
      ? std::optional<size_t>(gsl::narrow<size_t>(*max_queue_size))
      : std::nullopt;

  const auto minimum_severity = utils::parseOptionalU64Property(context, MinimumSeverity);
  subscription_options_.event_filter.minimum_severity = minimum_severity && *minimum_severity > 0 ? minimum_severity : std::nullopt;

  subscription_options_.event_filter.event_type_node_id = utils::parseProperty(context, EventTypeNodeId);
  subscription_options_.event_filter.filter_expression = context.getProperty(EventFilterExpression).value_or("");

  subscription_options_.event_filter.select_fields.clear();
  for (const auto& select_field : utils::string::splitAndTrimRemovingEmpty(utils::parseProperty(context, SelectFields), ",")) {
    subscription_options_.event_filter.select_fields.push_back(select_field);
  }
  if (subscription_options_.event_filter.select_fields.empty() && subscription_options_.event_filter.filter_expression.empty()) {
    throw Exception(PROCESS_SCHEDULE_EXCEPTION,
        fmt::format("At least one field must be set in '{}', otherwise the events would carry no data", SelectFields.name));
  }

  const auto batch_size = utils::parseOptionalU64Property(context, BatchSize);
  batch_size_ = batch_size && *batch_size > 0 ? std::optional<uint64_t>(*batch_size) : std::nullopt;

  {
    const std::lock_guard<std::mutex> lock(connection_mutex_);
    connection_ = opc::Client::createClient(logger_, application_uri_, cert_buffer_, key_buffer_, trust_buffers_);
    if (!connection_) {
      throw Exception(PROCESS_SCHEDULE_EXCEPTION, "Failed to create the OPC UA client");
    }
  }

  gsl_Expects(!event_thread_);
  event_thread_ = std::make_unique<utils::StoppableThread>([this]() { runEventLoop(); });
}

void FetchOPCEvents::runEventLoop() {
  while (!utils::StoppableThread::waitForStopRequest(0ms)) {
    try {
      if (!reconnect()) {
        utils::StoppableThread::waitForStopRequest(RETRY_INTERVAL);
        continue;
      }

      if (id_type_ == opc::OPCNodeIDType::Path && !path_node_id_resolved_) {
        std::vector<opc::NodeId> translated_node_ids;
        if (auto sc = connection_
                ->translateBrowsePathsToNodeIdsRequest(node_id_, translated_node_ids, namespace_idx_, path_reference_types_, logger_);
            sc != UA_STATUSCODE_GOOD) {
          logger_->log_error("Failed to translate path '{}' to a node id: {}", node_id_, UA_StatusCode_name(sc));
          utils::StoppableThread::waitForStopRequest(RETRY_INTERVAL);
          continue;
        }
        if (translated_node_ids.size() != 1) {
          logger_->log_error("Path '{}' resolved to {} node ids; exactly one is required to subscribe to events",
              node_id_,
              translated_node_ids.size());
          utils::StoppableThread::waitForStopRequest(RETRY_INTERVAL);
          continue;
        }
        node_ = std::move(translated_node_ids[0]);
        path_node_id_resolved_ = true;
      }

      if (!connection_->hasEventSubscription()) {
        if (auto sc = connection_->subscribeToEvents(node_, subscription_options_); sc != UA_STATUSCODE_GOOD) {
          logger_->log_error("Failed to subscribe to the events of node '{}': {}", node_id_, UA_StatusCode_name(sc));
          utils::StoppableThread::waitForStopRequest(RETRY_INTERVAL);
          continue;
        }
        logger_->log_debug("Subscribed to the events of node '{}'", node_id_);
      }

      // Receives the event notifications that arrived since the last call and queues them for onTrigger to take.
      if (auto sc = connection_->processSubscriptionNotifications(NOTIFICATION_WAIT_TIME_MS); sc != UA_STATUSCODE_GOOD) {
        logger_->log_error("Failed to process the OPC UA event notifications: {}", UA_StatusCode_name(sc));
        utils::StoppableThread::waitForStopRequest(RETRY_INTERVAL);
      }
    } catch (const std::exception& ex) {
      logger_->log_error("Exception while receiving OPC UA events: {}", ex.what());
      utils::StoppableThread::waitForStopRequest(RETRY_INTERVAL);
    }
  }
}

void FetchOPCEvents::stopEventThread() {
  event_thread_.reset();
  const std::lock_guard<std::mutex> lock(connection_mutex_);
  connection_.reset();
}

void FetchOPCEvents::notifyStop() {
  stopEventThread();
}

FetchOPCEvents::~FetchOPCEvents() {
  stopEventThread();
}

void FetchOPCEvents::createFlowFiles(core::ProcessSession& session, const std::vector<opc::Event>& events) const {
  core::RecordSet record_set;
  size_t events_pushed = 0;
  const auto writeToFlowFile = [&, this]() {
    auto flow_file = session.create();
    record_set_writer_->write(record_set, flow_file, session);

    session.transfer(flow_file, Success);
    events_pushed = 0;
    record_set.clear();
  };

  for (const auto& event : events) {
    core::Record record;
    for (const auto& [name, value] : event.fields) {
      record.emplace(name, core::RecordField(value));
    }
    record_set.push_back(std::move(record));
    ++events_pushed;

    if (batch_size_ && *batch_size_ > 0 && events_pushed >= *batch_size_) {
      writeToFlowFile();
    }
  }

  if (events_pushed > 0) {
    writeToFlowFile();
  }
}

void FetchOPCEvents::onTrigger(core::ProcessContext& context, core::ProcessSession& session) {
  logger_->log_trace("FetchOPCEvents::onTrigger");

  std::vector<opc::Event> events;
  uint64_t dropped_event_count = 0;
  {
    const std::lock_guard<std::mutex> lock(connection_mutex_);
    if (!connection_) {
      context.yield();
      return;
    }
    dropped_event_count = connection_->getDroppedEventCountSinceLastCall();
    events = connection_->drainEvents();
  }

  if (dropped_event_count > 0) {
    logger_->log_warn("The event queue is full, {} OPC UA events were dropped. Consider increasing '{}' or triggering the processor more often.",
        dropped_event_count,
        MaxQueueSize.name);
  }

  if (events.empty()) {
    logger_->log_debug("No OPC UA events were received");
    context.yield();
    return;
  }

  createFlowFiles(session, events);
  logger_->log_debug("Transferred {} OPC UA events", events.size());
}

REGISTER_RESOURCE(FetchOPCEvents, Processor);

}  // namespace org::apache::nifi::minifi::processors
