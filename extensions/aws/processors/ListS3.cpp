/**
 * @file ListS3.cpp
 * ListS3 class implementation
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

#include "ListS3.h"

#include "utils/StringUtils.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace aws {
namespace processors {

const core::Property ListS3::Delimiter(
  core::PropertyBuilder::createProperty("Delimiter")
    ->withDescription("The string used to delimit directories within the bucket. Please consult the AWS documentation for the correct use of this field.")
    ->build());
const core::Property ListS3::Prefix(
  core::PropertyBuilder::createProperty("Prefix")
    ->withDescription("The prefix used to filter the object list. In most cases, it should end with a forward slash ('/').")
    ->build());
const core::Property ListS3::UseVersions(
  core::PropertyBuilder::createProperty("Use Versions")
    ->isRequired(true)
    ->withDefaultValue<bool>(false)
    ->withDescription("Specifies whether to use S3 versions, if applicable. If false, only the latest version of each object will be returned.")
    ->build());
const core::Property ListS3::MinimumObjectAge(
  core::PropertyBuilder::createProperty("Minimum Object Age")
    ->isRequired(true)
    ->withDefaultValue<core::TimePeriodValue>("0 sec")
    ->withDescription("The minimum age that an S3 object must be in order to be considered; any object younger than this amount of time (according to last modification date) will be ignored.")
    ->build());
const core::Property ListS3::WriteObjectTags(
  core::PropertyBuilder::createProperty("Write Object Tags")
    ->isRequired(true)
    ->withDefaultValue<bool>(false)
    ->withDescription("If set to 'True', the tags associated with the S3 object will be written as FlowFile attributes")
    ->build());
const core::Property ListS3::WriteUserMetadata(
  core::PropertyBuilder::createProperty("Write User Metadata")
    ->isRequired(true)
    ->withDefaultValue<bool>(false)
    ->withDescription("If set to 'True', the user defined metadata associated with the S3 object will be added to FlowFile attributes/records")
    ->build());
const core::Property ListS3::RequesterPays(
  core::PropertyBuilder::createProperty("Requester Pays")
    ->isRequired(true)
    ->withDefaultValue<bool>(false)
    ->withDescription("If true, indicates that the requester consents to pay any charges associated with listing the S3 bucket. This sets the 'x-amz-request-payer' header to 'requester'. "
                      "Note that this setting is only used if Write User Metadata is true.")
    ->build());

const core::Relationship ListS3::Success("success", "FlowFiles are routed to success relationship");

void ListS3::initialize() {
  // Set the supported properties
  std::set<core::Property> properties(S3Processor::getSupportedProperties());
  properties.insert(Delimiter);
  properties.insert(Prefix);
  properties.insert(UseVersions);
  properties.insert(MinimumObjectAge);
  properties.insert(WriteObjectTags);
  properties.insert(WriteUserMetadata);
  properties.insert(RequesterPays);
  setSupportedProperties(properties);
  // Set the supported relationships
  std::set<core::Relationship> relationships;
  relationships.insert(Success);
  setSupportedRelationships(relationships);
}

void ListS3::onSchedule(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::ProcessSessionFactory> &sessionFactory) {
  S3Processor::onSchedule(context, sessionFactory);
  if (!getExpressionLanguageSupportedProperties(context, nullptr)) {
    throw Exception(PROCESS_SCHEDULE_EXCEPTION, "Required property is not set or invalid");
  }
  list_request_params_.bucket = std::move(bucket_);

  context->getProperty(Delimiter.getName(), list_request_params_.delimiter);
  logger_->log_debug("ListS3: Delimiter [%s]", list_request_params_.delimiter);

  context->getProperty(Prefix.getName(), list_request_params_.prefix);
  logger_->log_debug("ListS3: Prefix [%s]", list_request_params_.prefix);

  context->getProperty(UseVersions.getName(), list_request_params_.use_versions);
  logger_->log_debug("ListS3: UseVersions [%s]", list_request_params_.use_versions ? "true" : "false");

  std::string min_obj_age_str;
  if (!context->getProperty(MinimumObjectAge.getName(), min_obj_age_str) || min_obj_age_str.empty() || !core::Property::getTimeMSFromString(min_obj_age_str, list_request_params_.min_object_age)) {
    throw Exception(PROCESS_SCHEDULE_EXCEPTION, "Minimum Object Age missing or invalid");
  }
  logger_->log_debug("S3Processor: Minimum Object Age [%d]", min_obj_age_str, list_request_params_.min_object_age);

  context->getProperty(WriteObjectTags.getName(), write_object_tags_);
  logger_->log_debug("ListS3: WriteObjectTags [%s]", write_object_tags_ ? "true" : "false");

  context->getProperty(WriteUserMetadata.getName(), write_user_metadata_);
  logger_->log_debug("ListS3: WriteUserMetadata [%s]", write_user_metadata_ ? "true" : "false");

  context->getProperty(RequesterPays.getName(), requester_pays_);
  logger_->log_debug("ListS3: RequesterPays [%s]", requester_pays_ ? "true" : "false");
}

void ListS3::writeObjectTags(
    const std::string& bucket,
    aws::s3::ListedObjectAttributes object,
    const std::shared_ptr<core::ProcessSession> &session,
    const std::shared_ptr<core::FlowFile> &flow_file) {
  if (!write_object_tags_) {
    return;
  }

  auto get_object_tags_result = s3_wrapper_->getObjectTags(bucket, object.filename, object.version);
  if (get_object_tags_result) {
    for (const auto& tag : get_object_tags_result.value()) {
      session->putAttribute(flow_file, "s3.tag." + tag.first, tag.second);
    }
  } else {
    logger_->log_warn("Failed to get object tags for object %s in bucket %s", object.filename, bucket);
  }
}

void ListS3::writeUserMetadata(
    const std::string& bucket,
    aws::s3::ListedObjectAttributes object,
    const std::shared_ptr<core::ProcessSession> &session,
    const std::shared_ptr<core::FlowFile> &flow_file) {
  if (!write_user_metadata_) {
    return;
  }

  aws::s3::GetObjectRequestParameters params;
  params.bucket = list_request_params_.bucket;
  params.object_key = object.filename;
  params.version = object.version;
  params.requester_pays = requester_pays_;
  auto get_object_tags_result = s3_wrapper_->getObject(params);
  if (get_object_tags_result) {
    for (const auto& metadata : get_object_tags_result->user_metadata_map) {
      session->putAttribute(flow_file, "s3.user.metadata." + metadata.first, metadata.second);
    }
  } else {
    logger_->log_warn("Failed to get object metadata for object %s in bucket %s", params.object_key, params.bucket);
  }
}

void ListS3::onTrigger(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::ProcessSession> &session) {
  logger_->log_debug("ListS3 onTrigger");

  auto aws_results = s3_wrapper_->listBucket(list_request_params_);
  if (!aws_results) {
    logger_->log_error("Failed to list S3 bucket %s", list_request_params_.bucket);
    context->yield();
    return;
  }

  for (const auto& object : aws_results.value()) {
    auto flow_file = session->create();
    session->putAttribute(flow_file, "s3.bucket", list_request_params_.bucket);
    session->putAttribute(flow_file, core::SpecialFlowAttribute::FILENAME, object.filename);
    session->putAttribute(flow_file, "s3.etag", object.etag);
    session->putAttribute(flow_file, "s3.isLatest", object.is_latest ? "true" : "false");
    session->putAttribute(flow_file, "s3.lastModified", std::to_string(object.last_modified));
    session->putAttribute(flow_file, "s3.length", std::to_string(object.length));
    session->putAttribute(flow_file, "s3.storeClass", object.store_class);
    if (!object.version.empty()) {
      session->putAttribute(flow_file, "s3.version", object.version);
    }
    writeObjectTags(list_request_params_.bucket, object, session, flow_file);
    writeUserMetadata(list_request_params_.bucket, object, session, flow_file);

    session->transfer(flow_file, Success);
  }
}

}  // namespace processors
}  // namespace aws
}  // namespace minifi
}  // namespace nifi
}  // namespace apache
}  // namespace org
