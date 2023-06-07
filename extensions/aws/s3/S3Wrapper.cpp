/**
 * @file S3Wrapper.cpp
 * S3Wrapper class implementation
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
#include "S3Wrapper.h"

#include <memory>
#include <utility>
#include <vector>

#include "S3ClientRequestSender.h"
#include "utils/StringUtils.h"
#include "utils/file/FileUtils.h"
#include "utils/gsl.h"
#include "utils/RegexUtils.h"
#include "aws/core/utils/HashingUtils.h"
#include "range/v3/algorithm/find_if.hpp"

namespace org::apache::nifi::minifi::aws::s3 {

void HeadObjectResult::setFilePaths(const std::string& key) {
  absolute_path = std::filesystem::path(key, std::filesystem::path::format::generic_format);
  path = absolute_path.parent_path();
  filename = absolute_path.filename();
}

S3Wrapper::S3Wrapper() : request_sender_(std::make_unique<S3ClientRequestSender>()) {
}

S3Wrapper::S3Wrapper(std::unique_ptr<S3RequestSender>&& request_sender) : request_sender_(std::move(request_sender)) {
}

Expiration S3Wrapper::getExpiration(const std::string& expiration) {
  minifi::utils::Regex expr("expiry-date=\"(.*)\", rule-id=\"(.*)\"");
  minifi::utils::SMatch matches;
  const bool matched = minifi::utils::regexMatch(expiration, matches, expr);
  if (!matched || matches.size() < 3)
    return Expiration{};
  return Expiration{matches[1], matches[2]};
}

std::string S3Wrapper::getEncryptionString(Aws::S3::Model::ServerSideEncryption encryption) {
  if (encryption == Aws::S3::Model::ServerSideEncryption::NOT_SET) {
    return "";
  }

  auto it = std::find_if(SERVER_SIDE_ENCRYPTION_MAP.begin(), SERVER_SIDE_ENCRYPTION_MAP.end(),
    [&](const std::pair<std::string, const Aws::S3::Model::ServerSideEncryption>& pair) {
      return pair.second == encryption;
    });
  if (it != SERVER_SIDE_ENCRYPTION_MAP.end()) {
    return it->first;
  }
  return "";
}

std::shared_ptr<Aws::StringStream> S3Wrapper::readFlowFileStream(const std::shared_ptr<io::InputStream>& stream, uint64_t read_limit, uint64_t& read_size_out) {
  std::vector<std::byte> buffer;
  buffer.resize(BUFFER_SIZE);
  auto data_stream = std::make_shared<Aws::StringStream>();
  uint64_t read_size = 0;
  while (read_size < read_limit) {
    const auto next_read_size = (std::min)(read_limit - read_size, BUFFER_SIZE);
    const auto read_ret = stream->read(gsl::make_span(buffer).subspan(0, next_read_size));
    if (io::isError(read_ret)) {
      throw StreamReadException("Reading flow file inputstream failed!");
    }
    if (read_ret > 0) {
      data_stream->write(reinterpret_cast<char*>(buffer.data()), gsl::narrow<std::streamsize>(next_read_size));
      read_size += read_ret;
    } else {
      break;
    }
  }
  read_size_out = read_size;
  return data_stream;
}

std::optional<PutObjectResult> S3Wrapper::putObject(const PutObjectRequestParameters& put_object_params, const std::shared_ptr<io::InputStream>& stream, uint64_t flow_size) {
  uint64_t read_size{};
  auto data_stream = readFlowFileStream(stream, flow_size, read_size);
  auto request = createPutObjectRequest<Aws::S3::Model::PutObjectRequest>(put_object_params);
  request.SetBody(data_stream);

  auto aws_result = request_sender_->sendPutObjectRequest(request, put_object_params.credentials, put_object_params.client_config, put_object_params.use_virtual_addressing);
  if (!aws_result) {
    return std::nullopt;
  }

  return createPutObjectResult(*aws_result);
}

std::optional<S3Wrapper::UploadPartsResult> S3Wrapper::uploadParts(const PutObjectRequestParameters& put_object_params, const std::shared_ptr<io::InputStream>& stream,
    MultipartUploadState upload_state) {
  stream->seek(upload_state.uploaded_size);
  S3Wrapper::UploadPartsResult result;
  result.upload_id = upload_state.upload_id;
  result.part_etags = upload_state.uploaded_etags;
  const auto flow_size = upload_state.full_size - upload_state.uploaded_size;
  const size_t part_count = flow_size % upload_state.part_size == 0 ? flow_size / upload_state.part_size : flow_size / upload_state.part_size + 1;
  size_t total_read = 0;
  const size_t start_part = upload_state.uploaded_parts + 1;
  const size_t last_part = start_part + part_count - 1;
  for (size_t i = start_part; i <= last_part; ++i) {
    uint64_t read_size{};
    const auto remaining = flow_size - total_read;
    const auto next_read_size = remaining < upload_state.part_size ? remaining : upload_state.part_size;
    auto stream_ptr = readFlowFileStream(stream, next_read_size, read_size);
    total_read += read_size;

    Aws::S3::Model::UploadPartRequest upload_part_request;
    upload_part_request.WithBucket(put_object_params.bucket)
      .WithKey(put_object_params.object_key)
      .WithPartNumber(i)
      .WithUploadId(upload_state.upload_id);
    upload_part_request.SetBody(stream_ptr);

    Aws::Utils::ByteBuffer part_md5(Aws::Utils::HashingUtils::CalculateMD5(*stream_ptr));
    upload_part_request.SetContentMD5(Aws::Utils::HashingUtils::Base64Encode(part_md5));

    auto upload_part_result = request_sender_->sendUploadPartRequest(upload_part_request, put_object_params.credentials, put_object_params.client_config, put_object_params.use_virtual_addressing);
    if (!upload_part_result) {
      logger_->log_error("Failed to upload part %d of %d of S3 object with key '%s'", i, last_part, put_object_params.object_key);
      return std::nullopt;
    }
    result.part_etags.push_back(upload_part_result->GetETag());
    upload_state.uploaded_etags.push_back(upload_part_result->GetETag());
    upload_state.uploaded_parts += 1;
    upload_state.uploaded_size += read_size;
    multipart_upload_storage_->storeState(put_object_params.bucket, put_object_params.object_key, upload_state);
    logger_->log_info("Uploaded part %d of %d S3 object with key '%s'", i, last_part, put_object_params.object_key);
  }

  multipart_upload_storage_->removeState(put_object_params.bucket, put_object_params.object_key);
  return result;
}

std::optional<Aws::S3::Model::CompleteMultipartUploadResult> S3Wrapper::completeMultipartUpload(const PutObjectRequestParameters& put_object_params,
    const S3Wrapper::UploadPartsResult& upload_parts_result) {
  Aws::S3::Model::CompleteMultipartUploadRequest complete_multipart_upload_request;
  complete_multipart_upload_request.WithBucket(put_object_params.bucket)
    .WithKey(put_object_params.object_key)
    .WithUploadId(upload_parts_result.upload_id);

  Aws::S3::Model::CompletedMultipartUpload completed_multipart_upload;
  for (size_t i = 0; i < upload_parts_result.part_etags.size(); ++i) {
    Aws::S3::Model::CompletedPart part;
    part.WithETag(upload_parts_result.part_etags[i])
      .WithPartNumber(i + 1);
    completed_multipart_upload.AddParts(part);
  }

  complete_multipart_upload_request.SetMultipartUpload(completed_multipart_upload);

  return request_sender_->sendCompleteMultipartUploadRequest(complete_multipart_upload_request, put_object_params.credentials,
    put_object_params.client_config, put_object_params.use_virtual_addressing);
}

bool S3Wrapper::multipartUploadExistsInS3(const PutObjectRequestParameters& put_object_params) {
  ListMultipartUploadsRequestParameters params(put_object_params.credentials, put_object_params.client_config);
  params.bucket = put_object_params.bucket;
  auto pending_uploads = listMultipartUploads(params);
  if (!pending_uploads) {
    return false;
  }

  return ranges::find_if(*pending_uploads, [&](const auto& upload) { return upload.key == put_object_params.object_key; }) != pending_uploads->end();
}

std::optional<MultipartUploadState> S3Wrapper::getMultipartUploadState(const PutObjectRequestParameters& put_object_params) {
  auto upload_state = multipart_upload_storage_->getState(put_object_params.bucket, put_object_params.object_key);
  if (!upload_state) {
    return std::nullopt;
  }
  if (!multipartUploadExistsInS3(put_object_params)) {
    logger_->log_info("Local upload state for object '%s' in bucket '%s' not found in S3, removing it from local cache.", put_object_params.object_key, put_object_params.bucket);
    multipart_upload_storage_->removeState(put_object_params.bucket, put_object_params.object_key);
    return std::nullopt;
  }
  return upload_state;
}

std::optional<PutObjectResult> S3Wrapper::putObjectMultipart(const PutObjectRequestParameters& put_object_params, const std::shared_ptr<io::InputStream>& stream,
    uint64_t flow_size, uint64_t multipart_size) {
  gsl_Expects(multipart_upload_storage_);
  if (auto upload_state = getMultipartUploadState(put_object_params)) {
    logger_->log_info("Found previous multipart upload state for %s in bucket %s, continuing upload", put_object_params.object_key, put_object_params.bucket);
    return uploadParts(put_object_params, stream, std::move(*upload_state))
      | minifi::utils::flatMap([&, this](const auto& upload_parts_result) { return completeMultipartUpload(put_object_params, upload_parts_result); })
      | minifi::utils::map([this](const auto& complete_multipart_upload_result) { return createPutObjectResult(complete_multipart_upload_result); });
  } else {
    logger_->log_debug("No previous multipart upload state was found for %s in bucket %s", put_object_params.object_key, put_object_params.bucket);
    auto request = createPutObjectRequest<Aws::S3::Model::CreateMultipartUploadRequest>(put_object_params);
    return request_sender_->sendCreateMultipartUploadRequest(request, put_object_params.credentials, put_object_params.client_config, put_object_params.use_virtual_addressing)
      | minifi::utils::flatMap([&, this](const auto& create_multipart_result) { return uploadParts(put_object_params, stream,
          MultipartUploadState{create_multipart_result.GetUploadId(), multipart_size, flow_size, Aws::Utils::DateTime::Now()}); })
      | minifi::utils::flatMap([&, this](const auto& upload_parts_result) { return completeMultipartUpload(put_object_params, upload_parts_result); })
      | minifi::utils::map([this](const auto& complete_multipart_upload_result) { return createPutObjectResult(complete_multipart_upload_result); });
  }
}

bool S3Wrapper::deleteObject(const DeleteObjectRequestParameters& params) {
  Aws::S3::Model::DeleteObjectRequest request;
  request.WithBucket(params.bucket)
    .WithKey(params.object_key);
  if (!params.version.empty()) {
    request.SetVersionId(params.version);
  }
  return request_sender_->sendDeleteObjectRequest(request, params.credentials, params.client_config);
}

int64_t S3Wrapper::writeFetchedBody(Aws::IOStream& source, const int64_t data_size, io::OutputStream& output) {
  std::vector<uint8_t> buffer(4096);
  size_t write_size = 0;
  if (data_size < 0) return 0;
  while (write_size < gsl::narrow<uint64_t>(data_size)) {
    const auto next_write_size = (std::min)(gsl::narrow<size_t>(data_size) - write_size, size_t{4096});
    if (!source.read(reinterpret_cast<char*>(buffer.data()), gsl::narrow<std::streamsize>(next_write_size))) {
      return -1;
    }
    const auto ret = output.write(buffer.data(), next_write_size);
    if (io::isError(ret)) {
      return -1;
    }
    write_size += next_write_size;
  }
  return gsl::narrow<int64_t>(write_size);
}

std::optional<GetObjectResult> S3Wrapper::getObject(const GetObjectRequestParameters& get_object_params, io::OutputStream& out_body) {
  auto request = createFetchObjectRequest<Aws::S3::Model::GetObjectRequest>(get_object_params);
  auto aws_result = request_sender_->sendGetObjectRequest(request, get_object_params.credentials, get_object_params.client_config);
  if (!aws_result) {
    return std::nullopt;
  }
  auto result = fillFetchObjectResult<Aws::S3::Model::GetObjectResult, GetObjectResult>(get_object_params, *aws_result);
  result.write_size = writeFetchedBody(aws_result->GetBody(), aws_result->GetContentLength(), out_body);
  return result;
}

void S3Wrapper::addListResults(const Aws::Vector<Aws::S3::Model::ObjectVersion>& content, const uint64_t min_object_age, std::vector<ListedObjectAttributes>& listed_objects) {
  for (const auto& version : content) {
    if (last_bucket_list_timestamp_ - min_object_age < gsl::narrow<uint64_t>(version.GetLastModified().Millis())) {
      logger_->log_debug("Object version '%s' of key '%s' skipped due to minimum object age filter", version.GetVersionId(), version.GetKey());
      continue;
    }

    ListedObjectAttributes attributes;
    attributes.etag = minifi::utils::StringUtils::removeFramingCharacters(version.GetETag(), '"');
    attributes.filename = version.GetKey();
    attributes.is_latest = version.GetIsLatest();
    attributes.last_modified = version.GetLastModified().UnderlyingTimestamp();
    attributes.length = version.GetSize();
    attributes.store_class = VERSION_STORAGE_CLASS_MAP.at(version.GetStorageClass());
    attributes.version = version.GetVersionId();
    listed_objects.push_back(attributes);
  }
}

void S3Wrapper::addListResults(const Aws::Vector<Aws::S3::Model::Object>& content, const uint64_t min_object_age, std::vector<ListedObjectAttributes>& listed_objects) {
  for (const auto& object : content) {
    if (last_bucket_list_timestamp_ - min_object_age < gsl::narrow<uint64_t>(object.GetLastModified().Millis())) {
      logger_->log_debug("Object with key '%s' skipped due to minimum object age filter", object.GetKey());
      continue;
    }

    ListedObjectAttributes attributes;
    attributes.etag = minifi::utils::StringUtils::removeFramingCharacters(object.GetETag(), '"');
    attributes.filename = object.GetKey();
    attributes.is_latest = true;
    attributes.last_modified = object.GetLastModified().UnderlyingTimestamp();
    attributes.length = object.GetSize();
    attributes.store_class = OBJECT_STORAGE_CLASS_MAP.at(object.GetStorageClass());
    listed_objects.push_back(attributes);
  }
}

std::optional<std::vector<ListedObjectAttributes>> S3Wrapper::listVersions(const ListRequestParameters& params) {
  auto request = createListRequest<Aws::S3::Model::ListObjectVersionsRequest>(params);
  std::vector<ListedObjectAttributes> attribute_list;
  std::optional<Aws::S3::Model::ListObjectVersionsResult> aws_result;
  do {
    aws_result = request_sender_->sendListVersionsRequest(request, params.credentials, params.client_config);
    if (!aws_result) {
      return std::nullopt;
    }
    const auto& versions = aws_result->GetVersions();
    logger_->log_debug("AWS S3 List operation returned %zu versions. This result is%s truncated.", versions.size(), aws_result->GetIsTruncated() ? "" : " not");
    addListResults(versions, params.min_object_age, attribute_list);
    if (aws_result->GetIsTruncated()) {
      request.WithKeyMarker(aws_result->GetNextKeyMarker())
        .WithVersionIdMarker(aws_result->GetNextVersionIdMarker());
    }
  } while (aws_result->GetIsTruncated());

  return attribute_list;
}

std::optional<std::vector<ListedObjectAttributes>> S3Wrapper::listObjects(const ListRequestParameters& params) {
  auto request = createListRequest<Aws::S3::Model::ListObjectsV2Request>(params);
  std::vector<ListedObjectAttributes> attribute_list;
  std::optional<Aws::S3::Model::ListObjectsV2Result> aws_result;
  do {
    aws_result = request_sender_->sendListObjectsRequest(request, params.credentials, params.client_config);
    if (!aws_result) {
      return std::nullopt;
    }
    const auto& objects = aws_result->GetContents();
    logger_->log_debug("AWS S3 List operation returned %zu objects. This result is%s truncated.", objects.size(), aws_result->GetIsTruncated() ? "" : "not");
    addListResults(objects, params.min_object_age, attribute_list);
    if (aws_result->GetIsTruncated()) {
      request.SetContinuationToken(aws_result->GetNextContinuationToken());
    }
  } while (aws_result->GetIsTruncated());

  return attribute_list;
}

std::optional<std::vector<ListedObjectAttributes>> S3Wrapper::listBucket(const ListRequestParameters& params) {
  last_bucket_list_timestamp_ = gsl::narrow<uint64_t>(Aws::Utils::DateTime::CurrentTimeMillis());
  if (params.use_versions) {
    return listVersions(params);
  }
  return listObjects(params);
}

std::optional<std::map<std::string, std::string>> S3Wrapper::getObjectTags(const GetObjectTagsParameters& params) {
  Aws::S3::Model::GetObjectTaggingRequest request;
  request.WithBucket(params.bucket)
    .WithKey(params.object_key);
  if (!params.version.empty()) {
    request.SetVersionId(params.version);
  }
  auto aws_result = request_sender_->sendGetObjectTaggingRequest(request, params.credentials, params.client_config);
  if (!aws_result) {
    return std::nullopt;
  }
  std::map<std::string, std::string> tags;
  for (const auto& tag : aws_result->GetTagSet()) {
    tags.emplace(tag.GetKey(), tag.GetValue());
  }
  return tags;
}

std::optional<HeadObjectResult> S3Wrapper::headObject(const HeadObjectRequestParameters& head_object_params) {
  auto request = createFetchObjectRequest<Aws::S3::Model::HeadObjectRequest>(head_object_params);
  auto aws_result = request_sender_->sendHeadObjectRequest(request, head_object_params.credentials, head_object_params.client_config);
  if (!aws_result) {
    return std::nullopt;
  }
  return fillFetchObjectResult<Aws::S3::Model::HeadObjectResult, HeadObjectResult>(head_object_params, aws_result.value());
}

template<typename ListRequest>
ListRequest S3Wrapper::createListRequest(const ListRequestParameters& params) {
  ListRequest request;
  request.WithBucket(params.bucket)
    .WithDelimiter(params.delimiter)
    .WithPrefix(params.prefix);
  return request;
}

template<typename FetchObjectRequest>
FetchObjectRequest S3Wrapper::createFetchObjectRequest(const GetObjectRequestParameters& get_object_params) {
  FetchObjectRequest request;
  request.WithBucket(get_object_params.bucket)
    .WithKey(get_object_params.object_key);
  if (!get_object_params.version.empty()) {
    request.SetVersionId(get_object_params.version);
  }
  if (get_object_params.requester_pays) {
    request.SetRequestPayer(Aws::S3::Model::RequestPayer::requester);
  }
  return request;
}

template<typename AwsResult, typename FetchObjectResult>
FetchObjectResult S3Wrapper::fillFetchObjectResult(const GetObjectRequestParameters& get_object_params, const AwsResult& fetch_object_result) {
  FetchObjectResult result;
  result.setFilePaths(get_object_params.object_key);
  result.mime_type = fetch_object_result.GetContentType();
  result.etag = minifi::utils::StringUtils::removeFramingCharacters(fetch_object_result.GetETag(), '"');
  result.expiration = getExpiration(fetch_object_result.GetExpiration());
  result.ssealgorithm = getEncryptionString(fetch_object_result.GetServerSideEncryption());
  result.version = fetch_object_result.GetVersionId();
  for (const auto& metadata : fetch_object_result.GetMetadata()) {
    result.user_metadata_map.emplace(metadata.first, metadata.second);
  }
  return result;
}

void S3Wrapper::addListMultipartUploadResults(const Aws::Vector<Aws::S3::Model::MultipartUpload>& uploads, std::optional<std::chrono::milliseconds> max_upload_age,
    std::vector<MultipartUpload>& filtered_uploads) {
  const auto now = Aws::Utils::DateTime::Now();
  for (const auto& upload : uploads) {
    if (max_upload_age && now - upload.GetInitiated() <= *max_upload_age) {
      logger_->log_debug("Multipart upload with key '%s' and upload id '%s' did not meet the age limit", upload.GetKey(), upload.GetUploadId());
      continue;
    }

    logger_->log_info("Multipart upload with key '%s' and upload id '%s' older than age limit, marked for abortion", upload .GetKey(), upload.GetUploadId());
    MultipartUpload filtered_upload;
    filtered_upload.key = upload.GetKey();
    filtered_upload.upload_id = upload.GetUploadId();
    filtered_uploads.push_back(filtered_upload);
  }
}

std::optional<std::vector<MultipartUpload>> S3Wrapper::listMultipartUploads(const ListMultipartUploadsRequestParameters& params) {
  std::vector<MultipartUpload> result;
  std::optional<Aws::S3::Model::ListMultipartUploadsResult> aws_result;
  Aws::S3::Model::ListMultipartUploadsRequest request;
  request.SetBucket(params.bucket);
  do {
    aws_result = request_sender_->sendListMultipartUploadsRequest(request, params.credentials, params.client_config, params.use_virtual_addressing);
    if (!aws_result) {
      return std::nullopt;
    }
    const auto& uploads = aws_result->GetUploads();
    logger_->log_debug("AWS S3 List operation returned %zu multipart uploads. This result is%s truncated.", uploads.size(), aws_result->GetIsTruncated() ? "" : " not");
    addListMultipartUploadResults(uploads, params.upload_max_age, result);
    if (aws_result->GetIsTruncated()) {
      request.SetKeyMarker(aws_result->GetNextKeyMarker());
    }
  } while (aws_result->GetIsTruncated());

  return result;
}

bool S3Wrapper::abortMultipartUpload(const AbortMultipartUploadRequestParameters& params) {
  Aws::S3::Model::AbortMultipartUploadRequest request;
  request.WithBucket(params.bucket)
    .WithKey(params.key)
    .WithUploadId(params.upload_id);
  return request_sender_->sendAbortMultipartUploadRequest(request, params.credentials, params.client_config, params.use_virtual_addressing);
}

void S3Wrapper::initailizeMultipartUploadStateStorage(const std::string& multipart_temp_dir, const std::string& state_id) {
  multipart_upload_storage_ = std::make_unique<MultipartUploadStateStorage>(multipart_temp_dir, state_id);
}

void S3Wrapper::ageOffLocalS3MultipartUploadStates(std::chrono::milliseconds multipart_upload_max_age_threshold) {
  multipart_upload_storage_->removeAgedStates(multipart_upload_max_age_threshold);
}

}  // namespace org::apache::nifi::minifi::aws::s3
