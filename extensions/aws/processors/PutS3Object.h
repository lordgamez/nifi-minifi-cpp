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

#include <algorithm>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <sstream>
#include <string>
#include <utility>
#include <vector>
#include <chrono>

#include "io/StreamPipe.h"
#include "S3Processor.h"
#include "utils/ArrayUtils.h"
#include "utils/gsl.h"
#include "utils/Id.h"

template<typename T>
class S3TestsFixture;

namespace org::apache::nifi::minifi::aws::processors {

class PutS3Object : public S3Processor {
 public:
  static const std::set<std::string> CANNED_ACLS;
  static const std::set<std::string> STORAGE_CLASSES;
  static const std::set<std::string> SERVER_SIDE_ENCRYPTIONS;

  EXTENSIONAPI static constexpr const char* Description = "Puts FlowFiles to an Amazon S3 Bucket. The upload uses either the PutS3Object method or the PutS3MultipartUpload method. "
      "The PutS3Object method sends the file in a single synchronous call, but it has a 5GB size limit. Larger files are sent using the PutS3MultipartUpload method. "
      "This multipart process saves state after each step so that a large upload can be resumed with minimal loss if the processor or cluster is stopped and restarted. "
      "A multipart upload consists of three steps: 1) initiate upload, 2) upload the parts, and 3) complete the upload. For multipart uploads, the processor saves state "
      "locally tracking the upload ID and parts uploaded, which must both be provided to complete the upload. The AWS libraries select an endpoint URL based on the AWS region, "
      "but this can be overridden with the 'Endpoint Override URL' property for use with other S3-compatible endpoints. The S3 API specifies that the maximum file size for a "
      "PutS3Object upload is 5GB. It also requires that parts in a multipart upload must be at least 5MB in size, except for the last part. These limits establish the bounds "
      "for the Multipart Upload Threshold and Part Size properties.";

  static const core::Property ObjectKey;
  static const core::Property ContentType;
  static const core::Property StorageClass;
  static const core::Property ServerSideEncryption;
  static const core::Property FullControlUserList;
  static const core::Property ReadPermissionUserList;
  static const core::Property ReadACLUserList;
  static const core::Property WriteACLUserList;
  static const core::Property CannedACL;
  static const core::Property UsePathStyleAccess;
  static const core::Property MultipartThreshold;
  static const core::Property MultipartPartSize;
  static const core::Property MultipartUploadAgeOffInterval;
  static const core::Property MultipartUploadMaxAgeThreshold;
  static const core::Property TemporaryDirectoryMultipartState;
  static auto properties() {
    return minifi::utils::array_cat(S3Processor::properties(), std::array{
      ObjectKey,
      ContentType,
      StorageClass,
      ServerSideEncryption,
      FullControlUserList,
      ReadPermissionUserList,
      ReadACLUserList,
      WriteACLUserList,
      CannedACL,
      UsePathStyleAccess,
      MultipartThreshold,
      MultipartPartSize,
      MultipartUploadAgeOffInterval,
      MultipartUploadMaxAgeThreshold,
      TemporaryDirectoryMultipartState
    });
  }

  EXTENSIONAPI static const core::Relationship Success;
  EXTENSIONAPI static const core::Relationship Failure;
  static auto relationships() { return std::array{Success, Failure}; }

  EXTENSIONAPI static constexpr bool SupportsDynamicProperties = true;
  EXTENSIONAPI static constexpr bool SupportsDynamicRelationships = false;
  EXTENSIONAPI static constexpr core::annotation::Input InputRequirement = core::annotation::Input::INPUT_REQUIRED;
  EXTENSIONAPI static constexpr bool IsSingleThreaded = false;

  ADD_COMMON_VIRTUAL_FUNCTIONS_FOR_PROCESSORS

  explicit PutS3Object(std::string name, const minifi::utils::Identifier& uuid = minifi::utils::Identifier())
    : S3Processor(std::move(name), uuid, core::logging::LoggerFactory<PutS3Object>::getLogger(uuid)) {
  }

  ~PutS3Object() override = default;

  void initialize() override;
  void onSchedule(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::ProcessSessionFactory> &sessionFactory) override;
  void onTrigger(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::ProcessSession> &session) override;

  class ReadCallback {
   public:
    ReadCallback(uint64_t flow_size, const minifi::aws::s3::PutObjectRequestParameters& options, aws::s3::S3Wrapper& s3_wrapper,
          uint64_t multipart_threshold, uint64_t multipart_size, core::logging::Logger& logger)
      : flow_size_(flow_size),
        options_(options),
        s3_wrapper_(s3_wrapper),
        multipart_threshold_(multipart_threshold),
        multipart_size_(multipart_size),
        logger_(logger) {
    }

    int64_t operator()(const std::shared_ptr<io::InputStream>& stream) {
      try {
        if (flow_size_ <= multipart_threshold_) {
          logger_.log_info("Uploading S3 Object '%s' in a single upload", options_.object_key);
          result_ = s3_wrapper_.putObject(options_, stream, flow_size_);
          return gsl::narrow<int64_t>(flow_size_);
        } else {
          logger_.log_info("S3 Object '%s' passes the multipart threshold, uploading it in multiple parts", options_.object_key);
          result_ = s3_wrapper_.putObjectMultipart(options_, stream, flow_size_, multipart_size_);
          return gsl::narrow<int64_t>(flow_size_);
        }
      } catch(const aws::s3::StreamReadException& ex) {
        logger_.log_error("Error occurred while uploading to S3: %s", ex.what());
        return -1;
      }
    }

    uint64_t flow_size_;
    const minifi::aws::s3::PutObjectRequestParameters& options_;
    aws::s3::S3Wrapper& s3_wrapper_;
    uint64_t multipart_threshold_;
    uint64_t multipart_size_;
    uint64_t read_size_ = 0;
    std::optional<minifi::aws::s3::PutObjectResult> result_;
    core::logging::Logger& logger_;
  };

 protected:
  static constexpr uint64_t MIN_PART_SIZE = 5_MiB;
  static constexpr uint64_t MAX_UPLOAD_SIZE = 5_GiB;

  friend class ::S3TestsFixture<PutS3Object>;

  explicit PutS3Object(const std::string& name, const minifi::utils::Identifier& uuid, std::unique_ptr<aws::s3::S3RequestSender> s3_request_sender)
    : S3Processor(name, uuid, core::logging::LoggerFactory<PutS3Object>::getLogger(uuid), std::move(s3_request_sender)) {
  }

  virtual uint64_t getMinPartSize() const {
    return MIN_PART_SIZE;
  }

  virtual uint64_t getMaxUploadSize() const {
    return MAX_UPLOAD_SIZE;
  }

  void fillUserMetadata(const std::shared_ptr<core::ProcessContext> &context);
  static std::string parseAccessControlList(const std::string &comma_separated_list);
  bool setCannedAcl(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::FlowFile> &flow_file, aws::s3::PutObjectRequestParameters &put_s3_request_params) const;
  bool setAccessControl(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::FlowFile> &flow_file, aws::s3::PutObjectRequestParameters &put_s3_request_params) const;
  void setAttributes(
    const std::shared_ptr<core::ProcessSession> &session,
    const std::shared_ptr<core::FlowFile> &flow_file,
    const aws::s3::PutObjectRequestParameters &put_s3_request_params,
    const minifi::aws::s3::PutObjectResult &put_object_result) const;
  std::optional<aws::s3::PutObjectRequestParameters> buildPutS3RequestParams(
    const std::shared_ptr<core::ProcessContext> &context,
    const std::shared_ptr<core::FlowFile> &flow_file,
    const CommonProperties &common_properties) const;
  void ageOffMultipartUploads(const CommonProperties &common_properties);

  std::string user_metadata_;
  std::map<std::string, std::string> user_metadata_map_;
  std::string storage_class_;
  std::string server_side_encryption_;
  bool use_virtual_addressing_ = true;
  uint64_t multipart_threshold_{};
  uint64_t multipart_size_{};
  std::chrono::milliseconds multipart_upload_ageoff_interval_;
  std::chrono::milliseconds multipart_upload_max_age_threshold_;
  std::chrono::time_point<std::chrono::system_clock> last_ageoff_time_;
};

}  // namespace org::apache::nifi::minifi::aws::processors
