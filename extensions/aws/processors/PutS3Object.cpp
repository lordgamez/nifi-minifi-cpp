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

#include "PutS3Object.h"

#include <string>
#include <set>
#include <memory>
#include <utility>

#include "AWSCredentialsService.h"
#include "properties/Properties.h"
#include "utils/StringUtils.h"
#include "utils/MapUtils.h"
#include "core/ProcessContext.h"
#include "core/ProcessSession.h"
#include "core/Resource.h"
#include "utils/ProcessorConfigUtils.h"

namespace org::apache::nifi::minifi::aws::processors {

void PutS3Object::initialize() {
  setSupportedProperties(properties());
  setSupportedRelationships(relationships());
}

void PutS3Object::fillUserMetadata(const std::shared_ptr<core::ProcessContext> &context) {
  const auto &dynamic_prop_keys = context->getDynamicPropertyKeys();
  bool first_property = true;
  for (const auto &prop_key : dynamic_prop_keys) {
    std::string prop_value;
    if (context->getDynamicProperty(prop_key, prop_value) && !prop_value.empty()) {
      logger_->log_debug("PutS3Object: DynamicProperty: [%s] -> [%s]", prop_key, prop_value);
      user_metadata_map_.emplace(prop_key, prop_value);
      if (first_property) {
        user_metadata_ = minifi::utils::StringUtils::join_pack(prop_key, "=", prop_value);
        first_property = false;
      } else {
        user_metadata_ += minifi::utils::StringUtils::join_pack(",", prop_key, "=", prop_value);
      }
    }
  }
  logger_->log_debug("PutS3Object: User metadata [%s]", user_metadata_);
}

void PutS3Object::onSchedule(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::ProcessSessionFactory> &sessionFactory) {
  S3Processor::onSchedule(context, sessionFactory);

  if (!context->getProperty(StorageClass.getName(), storage_class_)
      || storage_class_.empty()
      || STORAGE_CLASSES.find(storage_class_) == STORAGE_CLASSES.end()) {
    throw Exception(PROCESS_SCHEDULE_EXCEPTION, "Storage Class property missing or invalid");
  }
  logger_->log_debug("PutS3Object: Storage Class [%s]", storage_class_);

  if (!context->getProperty(ServerSideEncryption.getName(), server_side_encryption_)
      || server_side_encryption_.empty()
      || SERVER_SIDE_ENCRYPTIONS.find(server_side_encryption_) == SERVER_SIDE_ENCRYPTIONS.end()) {
    throw Exception(PROCESS_SCHEDULE_EXCEPTION, "Server Side Encryption property missing or invalid");
  }
  logger_->log_debug("PutS3Object: Server Side Encryption [%s]", server_side_encryption_);

  if (auto use_path_style_access = context->getProperty<bool>(UsePathStyleAccess)) {
    use_virtual_addressing_ = !*use_path_style_access;
  }

  context->getProperty(MultipartThreshold.getName(), multipart_threshold_);
  if (multipart_threshold_ > getMaxUploadSize() || multipart_threshold_ < getMinPartSize()) {
    throw Exception(PROCESS_SCHEDULE_EXCEPTION, "Multipart Threshold is not between the valid 5MB and 5GB range!");
  }
  logger_->log_debug("PutS3Object: Multipart Threshold %" PRIu64, multipart_threshold_);
  context->getProperty(MultipartPartSize.getName(), multipart_size_);
  if (multipart_size_ > getMaxUploadSize() || multipart_size_ < getMinPartSize()) {
    throw Exception(PROCESS_SCHEDULE_EXCEPTION, "Multipart Part Size is not between the valid 5MB and 5GB range!");
  }
  logger_->log_debug("PutS3Object: Multipart Size %" PRIu64, multipart_size_);


  multipart_upload_ageoff_interval_ = minifi::utils::getRequiredPropertyOrThrow<core::TimePeriodValue>(*context, MultipartUploadAgeOffInterval.getName()).getMilliseconds();
  logger_->log_debug("PutS3Object: Multipart Upload Ageoff Interval %" PRId64 " ms", int64_t{multipart_upload_ageoff_interval_.count()});

  multipart_upload_max_age_threshold_ = minifi::utils::getRequiredPropertyOrThrow<core::TimePeriodValue>(*context, MultipartUploadMaxAgeThreshold.getName()).getMilliseconds();
  logger_->log_debug("PutS3Object: Multipart Upload Max Age Threshold %" PRId64 " ms", int64_t{multipart_upload_max_age_threshold_.count()});

  fillUserMetadata(context);

  std::string multipart_temp_dir;
  context->getProperty(TemporaryDirectoryMultipartState.getName(), multipart_temp_dir);


  s3_wrapper_.initializeMultipartUploadStateStorage(multipart_temp_dir, getUUIDStr());
}

std::string PutS3Object::parseAccessControlList(const std::string &comma_separated_list) {
  auto users = minifi::utils::StringUtils::split(comma_separated_list, ",");
  for (auto& user : users) {
    auto trimmed_user = minifi::utils::StringUtils::trim(user);
    if (trimmed_user.find('@') != std::string::npos) {
      user = "emailAddress=\"" + trimmed_user + "\"";
    } else {
      user = "id=" + trimmed_user;
    }
  }
  return minifi::utils::StringUtils::join(", ", users);
}

bool PutS3Object::setCannedAcl(
    const std::shared_ptr<core::ProcessContext> &context,
    const std::shared_ptr<core::FlowFile> &flow_file,
    aws::s3::PutObjectRequestParameters &put_s3_request_params) const {
  context->getProperty(CannedACL, put_s3_request_params.canned_acl, flow_file);
  if (!put_s3_request_params.canned_acl.empty() && CANNED_ACLS.find(put_s3_request_params.canned_acl) == CANNED_ACLS.end()) {
    logger_->log_error("Canned ACL is invalid!");
    return false;
  }
  logger_->log_debug("PutS3Object: Canned ACL [%s]", put_s3_request_params.canned_acl);
  return true;
}

bool PutS3Object::setAccessControl(
      const std::shared_ptr<core::ProcessContext> &context,
      const std::shared_ptr<core::FlowFile> &flow_file,
      aws::s3::PutObjectRequestParameters &put_s3_request_params) const {
  std::string value;
  if (context->getProperty(FullControlUserList, value, flow_file) && !value.empty()) {
    put_s3_request_params.fullcontrol_user_list = parseAccessControlList(value);
    logger_->log_debug("PutS3Object: Full Control User List [%s]", value);
  }
  if (context->getProperty(ReadPermissionUserList, value, flow_file) && !value.empty()) {
    put_s3_request_params.read_permission_user_list = parseAccessControlList(value);
    logger_->log_debug("PutS3Object: Read Permission User List [%s]", value);
  }
  if (context->getProperty(ReadACLUserList, value, flow_file) && !value.empty()) {
    put_s3_request_params.read_acl_user_list = parseAccessControlList(value);
    logger_->log_debug("PutS3Object: Read ACL User List [%s]", value);
  }
  if (context->getProperty(WriteACLUserList, value, flow_file) && !value.empty()) {
    put_s3_request_params.write_acl_user_list = parseAccessControlList(value);
    logger_->log_debug("PutS3Object: Write ACL User List [%s]", value);
  }

  return setCannedAcl(context, flow_file, put_s3_request_params);
}

std::optional<aws::s3::PutObjectRequestParameters> PutS3Object::buildPutS3RequestParams(
    const std::shared_ptr<core::ProcessContext> &context,
    const std::shared_ptr<core::FlowFile> &flow_file,
    const CommonProperties &common_properties) const {
  gsl_Expects(client_config_);
  aws::s3::PutObjectRequestParameters params(common_properties.credentials, *client_config_);
  params.setClientConfig(common_properties.proxy, common_properties.endpoint_override_url);
  params.bucket = common_properties.bucket;
  params.user_metadata_map = user_metadata_map_;
  params.server_side_encryption = server_side_encryption_;
  params.storage_class = storage_class_;

  context->getProperty(ObjectKey, params.object_key, flow_file);
  if (params.object_key.empty() && (!flow_file->getAttribute("filename", params.object_key) || params.object_key.empty())) {
    logger_->log_error("No Object Key is set and default object key 'filename' attribute could not be found!");
    return std::nullopt;
  }
  logger_->log_debug("PutS3Object: Object Key [%s]", params.object_key);

  context->getProperty(ContentType, params.content_type, flow_file);
  logger_->log_debug("PutS3Object: Content Type [%s]", params.content_type);

  if (!setAccessControl(context, flow_file, params)) {
    return std::nullopt;
  }

  params.use_virtual_addressing = use_virtual_addressing_;
  return params;
}

void PutS3Object::setAttributes(
    const std::shared_ptr<core::ProcessSession> &session,
    const std::shared_ptr<core::FlowFile> &flow_file,
    const aws::s3::PutObjectRequestParameters &put_s3_request_params,
    const minifi::aws::s3::PutObjectResult &put_object_result) const {
  session->putAttribute(flow_file, "s3.bucket", put_s3_request_params.bucket);
  session->putAttribute(flow_file, "s3.key", put_s3_request_params.object_key);
  session->putAttribute(flow_file, "s3.contenttype", put_s3_request_params.content_type);

  if (!user_metadata_.empty()) {
    session->putAttribute(flow_file, "s3.usermetadata", user_metadata_);
  }
  if (!put_object_result.version.empty()) {
    session->putAttribute(flow_file, "s3.version", put_object_result.version);
  }
  if (!put_object_result.etag.empty()) {
    session->putAttribute(flow_file, "s3.etag", put_object_result.etag);
  }
  if (!put_object_result.expiration.empty()) {
    session->putAttribute(flow_file, "s3.expiration", put_object_result.expiration);
  }
  if (!put_object_result.ssealgorithm.empty()) {
    session->putAttribute(flow_file, "s3.sseAlgorithm", put_object_result.ssealgorithm);
  }
}

void PutS3Object::ageOffMultipartUploads(const CommonProperties &common_properties) {
  const auto now = std::chrono::system_clock::now();
  if (now - last_ageoff_time_ < multipart_upload_ageoff_interval_) {
    logger_->log_debug("Multipart Upload Age off interval still in progress, not checking obsolete multipart uploads.");
    return;
  }

  logger_->log_trace("Listing aged off multipart uploads still in progress.");
  aws::s3::ListMultipartUploadsRequestParameters list_params(common_properties.credentials, *client_config_);
  list_params.setClientConfig(common_properties.proxy, common_properties.endpoint_override_url);
  list_params.bucket = common_properties.bucket;
  list_params.upload_max_age = multipart_upload_max_age_threshold_;
  list_params.use_virtual_addressing = use_virtual_addressing_;
  auto aged_off_uploads_in_progress = s3_wrapper_.listMultipartUploads(list_params);
  if (!aged_off_uploads_in_progress) {
    logger_->log_error("Listing aged off multipart uploads failed!");
    return;
  }

  logger_->log_info("Found %d aged off pending multipart upload jobs in bucket '%s'", aged_off_uploads_in_progress->size(), common_properties.bucket);
  size_t aborted = 0;
  for (const auto& upload : *aged_off_uploads_in_progress) {
    logger_->log_info("Aborting multipart upload with key '%s' and upload id '%s' in bucket '%s'", upload.key, upload.upload_id, common_properties.bucket);
    aws::s3::AbortMultipartUploadRequestParameters abort_params(common_properties.credentials, *client_config_);
    abort_params.setClientConfig(common_properties.proxy, common_properties.endpoint_override_url);
    abort_params.bucket = common_properties.bucket;
    abort_params.key = upload.key;
    abort_params.upload_id = upload.upload_id;
    abort_params.use_virtual_addressing = use_virtual_addressing_;
    if (!s3_wrapper_.abortMultipartUpload(abort_params)) {
       logger_->log_error("Failed to abort multipart upload with key '%s' and upload id '%s' in bucket '%s'", abort_params.key, abort_params.upload_id, abort_params.bucket);
       continue;
    }
    ++aborted;
  }
  if (aborted > 0) {
    logger_->log_info("Aborted %d pending multipart upload jobs in bucket '%s'", aborted, common_properties.bucket);
  }
  s3_wrapper_.ageOffLocalS3MultipartUploadStates(multipart_upload_max_age_threshold_);
  last_ageoff_time_ = now;
}

void PutS3Object::onTrigger(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::ProcessSession> &session) {
  logger_->log_trace("PutS3Object onTrigger");
  std::shared_ptr<core::FlowFile> flow_file = session->get();
  if (!flow_file) {
    context->yield();
    return;
  }

  auto common_properties = getCommonELSupportedProperties(context, flow_file);
  if (!common_properties) {
    session->transfer(flow_file, Failure);
    return;
  }

  ageOffMultipartUploads(*common_properties);

  auto put_s3_request_params = buildPutS3RequestParams(context, flow_file, *common_properties);
  if (!put_s3_request_params) {
    session->transfer(flow_file, Failure);
    return;
  }

  PutS3Object::ReadCallback callback(flow_file->getSize(), *put_s3_request_params, s3_wrapper_, multipart_threshold_, multipart_size_, *logger_);
  session->read(flow_file, std::ref(callback));
  if (!callback.result_.has_value()) {
    logger_->log_error("Failed to upload S3 object to bucket '%s'", put_s3_request_params->bucket);
    session->transfer(flow_file, Failure);
  } else {
    setAttributes(session, flow_file, *put_s3_request_params, *callback.result_);
    logger_->log_debug("Successfully uploaded S3 object '%s' to bucket '%s'", put_s3_request_params->object_key, put_s3_request_params->bucket);
    session->transfer(flow_file, Success);
  }
}

}  // namespace org::apache::nifi::minifi::aws::processors
