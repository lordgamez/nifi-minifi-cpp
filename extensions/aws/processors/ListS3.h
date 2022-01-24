/**
 * @file ListS3.h
 * ListS3 class declaration
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

#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <utility>
#include <tuple>
#include <memory>

#include "S3Processor.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace aws {
namespace processors {

class ListS3 : public S3Processor {
 public:
  static constexpr char const* ProcessorName = "ListS3";

  // Supported Properties
  static const core::Property Delimiter;
  static const core::Property Prefix;
  static const core::Property UseVersions;
  static const core::Property MinimumObjectAge;
  static const core::Property WriteObjectTags;
  static const core::Property WriteUserMetadata;
  static const core::Property RequesterPays;

  // Supported Relationships
  static const core::Relationship Success;

  explicit ListS3(const std::string& name, const minifi::utils::Identifier& uuid = minifi::utils::Identifier())
    : S3Processor(name, uuid, core::logging::LoggerFactory<ListS3>::getLogger()) {
  }
  explicit ListS3(const std::string& name, minifi::utils::Identifier uuid, std::unique_ptr<aws::s3::S3RequestSender> s3_request_sender)
    : S3Processor(name, uuid, core::logging::LoggerFactory<ListS3>::getLogger(), std::move(s3_request_sender)) {
  }

  ~ListS3() override = default;

  void initialize() override;
  void onSchedule(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::ProcessSessionFactory> &sessionFactory) override;
  void onTrigger(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::ProcessSession> &session) override;

 private:
  core::annotation::Input getInputRequirement() const override {
    return core::annotation::Input::INPUT_FORBIDDEN;
  }

  void writeObjectTags(
    const aws::s3::ListedObjectAttributes &object_attributes,
    core::ProcessSession &session,
    const std::shared_ptr<core::FlowFile> &flow_file);
  void writeUserMetadata(
    const aws::s3::ListedObjectAttributes &object_attributes,
    core::ProcessSession &session,
    const std::shared_ptr<core::FlowFile> &flow_file);
  void createNewFlowFile(
    core::ProcessSession &session,
    const aws::s3::ListedObjectAttributes &object_attributes);

  std::unique_ptr<aws::s3::ListRequestParameters> list_request_params_;
  bool write_object_tags_ = false;
  bool write_user_metadata_ = false;
  bool requester_pays_ = false;
  std::unique_ptr<minifi::utils::ListingStateManager> state_manager_;
};

}  // namespace processors
}  // namespace aws
}  // namespace minifi
}  // namespace nifi
}  // namespace apache
}  // namespace org
