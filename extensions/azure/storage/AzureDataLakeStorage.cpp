/**
 * @file AzureDataLakeStorage.cpp
 * AzureDataLakeStorage class implementation
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

#include "AzureDataLakeStorage.h"

#include "AzureDataLakeStorageClient.h"

namespace org::apache::nifi::minifi::azure::storage {

AzureDataLakeStorage::AzureDataLakeStorage(std::unique_ptr<DataLakeStorageClient> data_lake_storage_client)
  : data_lake_storage_client_(data_lake_storage_client ? std::move(data_lake_storage_client) : std::make_unique<AzureDataLakeStorageClient>()) {
}

UploadDataLakeStorageResult AzureDataLakeStorage::uploadFile(const PutAzureDataLakeStorageParameters& params, gsl::span<const uint8_t> buffer) {
  UploadDataLakeStorageResult result;
  logger_->log_debug("Uploading file '%s/%s' to Azure Data Lake Storage filesystem '%s'", params.directory_name, params.filename, params.file_system_name);
  try {
    auto file_created = data_lake_storage_client_->createFile(params);
    if (!file_created && !params.replace_file) {
      logger_->log_warn("File '%s/%s' already exists on Azure Data Lake Storage filesystem '%s'", params.directory_name, params.filename, params.file_system_name);
      result.result_code = UploadResultCode::FILE_ALREADY_EXISTS;
      return result;
    }

    auto upload_url = data_lake_storage_client_->uploadFile(params, buffer);
    if (auto query_string_pos = upload_url.find('?'); query_string_pos != std::string::npos) {
      upload_url = upload_url.substr(0, query_string_pos);
    }
    result.primary_uri = upload_url;
    return result;
  } catch(const std::exception& ex) {
    logger_->log_error("An exception occurred while uploading file to Azure Data Lake storage: %s", ex.what());
    result.result_code = UploadResultCode::FAILURE;
    return result;
  }
}

bool AzureDataLakeStorage::deleteFile(const DeleteAzureDataLakeStorageParameters& params) {
  try {
    return data_lake_storage_client_->deleteFile(params);
  } catch (const std::exception& ex) {
    logger_->log_error("An exception occurred while deleting '%s/%s' of filesystem '%s': %s", params.directory_name, params.filename, params.file_system_name, ex.what());
    return false;
  }
}

std::optional<uint64_t> AzureDataLakeStorage::fetchFile(const FetchAzureDataLakeStorageParameters& params, io::BaseStream& stream) {
  try {
    auto fetch_res = data_lake_storage_client_->fetchFile(params);

    std::vector<uint8_t> buffer(4096);
    size_t write_size = 0;
    if (fetch_res.FileSize < 0) return 0;
    while (write_size < gsl::narrow<uint64_t>(fetch_res.FileSize)) {
      const auto next_write_size = (std::min)(gsl::narrow<size_t>(fetch_res.FileSize) - write_size, size_t{4096});
      if (!fetch_res.Body->Read(buffer.data(), gsl::narrow<std::streamsize>(next_write_size))) {
        return -1;
      }
      const auto ret = stream.write(buffer.data(), next_write_size);
      if (io::isError(ret)) {
        return -1;
      }
      write_size += next_write_size;
    }
    return gsl::narrow<int64_t>(write_size);
  } catch (const std::exception& ex) {
    logger_->log_error("An exception occurred while fetching '%s/%s' of filesystem '%s': %s", params.directory_name, params.filename, params.file_system_name, ex.what());
    return std::nullopt;
  }
}

}  // namespace org::apache::nifi::minifi::azure::storage
