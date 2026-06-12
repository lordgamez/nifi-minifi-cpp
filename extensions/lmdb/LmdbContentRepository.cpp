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

#include "LmdbContentRepository.h"

#include <cinttypes>
#include <memory>
#include <string>
#include <utility>
#include <vector>
#include <filesystem>

#include "minifi-cpp/Exception.h"
#include "core/Resource.h"
#include "core/TypedValues.h"
#include "utils/Locations.h"
#include "minifi-cpp/utils/gsl.h"
#include "LmdbStream.h"
#include "lmdb.h"

namespace org::apache::nifi::minifi::core::repository {

LmdbContentRepository::Session::Session(std::shared_ptr<ContentRepository> repository)
    : BufferedContentSession(std::move(repository)) {}

void LmdbContentRepository::Session::commit() {
  auto lmdbContentRepository = std::dynamic_pointer_cast<LmdbContentRepository>(repository_);
  if (!lmdbContentRepository) {
    throw Exception(REPOSITORY_EXCEPTION, "Session's repository is not an LmdbContentRepository");
  }

  for (const auto& resource : managed_resources_) {
    auto outStream = lmdbContentRepository->write(*resource.first, false);
    if (outStream == nullptr) {
      throw Exception(REPOSITORY_EXCEPTION, "Couldn't open the underlying resource for write: " + resource.first->getContentFullPath());
    }
    const auto size = resource.second->size();
    if (outStream->write(resource.second->getBuffer()) != size) {
      throw Exception(REPOSITORY_EXCEPTION, "Failed to write new resource: " + resource.first->getContentFullPath());
    }
    auto lmdb_out_stream = std::dynamic_pointer_cast<io::LmdbStream>(outStream);
    if (lmdb_out_stream == nullptr) {
      throw Exception(REPOSITORY_EXCEPTION, "Couldn't cast output stream to LmdbStream for commit: " + resource.first->getContentFullPath());
    }
    if (!lmdb_out_stream->commit()) {
      throw Exception(REPOSITORY_EXCEPTION, "Failed to commit new resource: " + resource.first->getContentFullPath());
    }
  }

  for (const auto& resource : append_state_) {
    auto outStream = lmdbContentRepository->write(*resource.first, true);
    if (outStream == nullptr) {
      throw Exception(REPOSITORY_EXCEPTION, "Couldn't open the underlying resource for append: " + resource.first->getContentFullPath());
    }
    const auto size = resource.second.stream->size();
    if (outStream->write(resource.second.stream->getBuffer()) != size) {
      throw Exception(REPOSITORY_EXCEPTION, "Failed to append to resource: " + resource.first->getContentFullPath());
    }
    auto lmdb_out_stream = std::dynamic_pointer_cast<io::LmdbStream>(outStream);
    if (lmdb_out_stream == nullptr) {
      throw Exception(REPOSITORY_EXCEPTION, "Couldn't cast output stream to LmdbStream for commit: " + resource.first->getContentFullPath());
    }
    if (!lmdb_out_stream->commit()) {
      throw Exception(REPOSITORY_EXCEPTION, "Failed to commit appended resource: " + resource.first->getContentFullPath());
    }
  }

  managed_resources_.clear();
  append_state_.clear();
}

bool LmdbContentRepository::initialize(const std::shared_ptr<minifi::Configure> &configuration) {
  if (const int rc = mdb_env_create(&lmdb_env_)) {
    logger_->log_error("Failed to create LMDB environment: {}", mdb_strerror(rc));
    return false;
  }

  // TODO(lordgamez): consider making these configurable if needed
  // Limit large enough to accommodate all our named dbs. This only starts to matter if the number gets large, otherwise it's just a bunch of extra entries in the main table.
  mdb_env_set_maxdbs(lmdb_env_, 4);

  // This is the maximum size of the db (but will not be used directly), so we make it large enough that we hopefully never run into the limit.
  mdb_env_set_mapsize(lmdb_env_, static_cast<size_t>(1048576) * static_cast<size_t>(100000));  // 1MB * 100000

  const auto working_dir = utils::getMinifiDir();

  std::string value;
  if (configuration->get(Configure::nifi_dbcontent_repository_directory_default, value) && !value.empty()) {
    directory_ = value;
  } else {
    directory_ = (working_dir / "lmdbcontentrepository").string();
  }
  if (std::filesystem::exists(directory_)) {
    logger_->log_info("Using existing LMDB Content Repository directory at {}", directory_);
  } else {
    logger_->log_info("Creating LMDB Content Repository directory at {}", directory_);
    if (!std::filesystem::create_directories(directory_)) {
      logger_->log_error("Failed to create LMDB Content Repository directory at {}", directory_);
      return false;
    }
  }
  if (const int rc = mdb_env_open(lmdb_env_, directory_.c_str(), MDB_NOTLS, 0664)) {
    logger_->log_error("Failed to open LMDB environment: {}", mdb_strerror(rc));
    return false;
  }

  MDB_txn* init_txn = nullptr;
  mdb_txn_begin(lmdb_env_, nullptr, 0, &init_txn);
  if (const int rc = mdb_dbi_open(init_txn, nullptr, 0, &lmdb_handle_); rc != MDB_SUCCESS) {
    logger_->log_error("Failed to open LMDB database: {}", mdb_strerror(rc));
    mdb_txn_abort(init_txn);
    mdb_env_close(lmdb_env_);
    return false;
  }
  mdb_txn_commit(init_txn);

  return true;
}

void LmdbContentRepository::start() {
  // TODO(lordgamez): add compaction thread if needed
}

void LmdbContentRepository::stop() {
}

std::shared_ptr<ContentSession> LmdbContentRepository::createSession() {
  return std::make_shared<Session>(sharedFromThis<ContentRepository>());
}

std::shared_ptr<io::BaseStream> LmdbContentRepository::write(const minifi::ResourceClaim &claim, bool) {
  return std::make_shared<io::LmdbStream>(claim.getContentFullPath(), lmdb_env_, &lmdb_handle_, true);
}

std::shared_ptr<io::BaseStream> LmdbContentRepository::read(const minifi::ResourceClaim &claim) {
  return std::make_shared<io::LmdbStream>(claim.getContentFullPath(), lmdb_env_, &lmdb_handle_, false);
}

bool LmdbContentRepository::exists(const minifi::ResourceClaim &streamId) {
  auto path = streamId.getContentFullPath();
  MDB_val key{ path.size(), const_cast<char*>(path.data()) };
  MDB_val value{};

  MDB_txn* txn = nullptr;
  mdb_txn_begin(lmdb_env_, nullptr, MDB_RDONLY, &txn);

  auto rc = mdb_get(txn, lmdb_handle_, &key, &value);

  bool exists = false;
  if (rc == MDB_SUCCESS) {
    exists = true;
  } else if (rc != MDB_NOTFOUND) {
    logger_->log_error("Failed to get value from LMDB database: {}", mdb_strerror(rc));
  }

  mdb_txn_abort(txn);
  return exists;
}

bool LmdbContentRepository::removeKey(const std::string& content_path) {
  MDB_val key{ content_path.size(), const_cast<char*>(content_path.data()) };

  MDB_txn* txn = nullptr;
  mdb_txn_begin(lmdb_env_, nullptr, 0, &txn);
  int rc = mdb_del(txn, lmdb_handle_, &key, nullptr);
  auto result = false;

  if (rc == MDB_SUCCESS) {
    result = true;
  } else if (rc == MDB_NOTFOUND) {
    logger_->log_debug("Key {} not found in LMDB database during delete", content_path);
  } else {
    logger_->log_error("Failed to delete key from LMDB database: {}", mdb_strerror(rc));
  }

  mdb_txn_commit(txn);

  return result;
}

void LmdbContentRepository::clearOrphans() {
  std::vector<std::string> keys_to_be_deleted;

  MDB_txn* txn = nullptr;
  mdb_txn_begin(lmdb_env_, nullptr, MDB_RDONLY, &txn);

  MDB_cursor* cursor;
  mdb_cursor_open(txn, lmdb_handle_, &cursor);

  MDB_val key{};
  MDB_val val{};
  int rc = mdb_cursor_get(cursor, &key, &val, MDB_FIRST);

  while (rc == MDB_SUCCESS) {
    std::string key_string = std::string(static_cast<char*>(key.mv_data), key.mv_size);

    std::lock_guard<std::mutex> lock(count_map_mutex_);
    auto claim_it = count_map_.find(key_string);
    if (claim_it == count_map_.end() || claim_it->second == 0) {
      logger_->log_error("Deleting orphan resource {}", key_string);
      keys_to_be_deleted.push_back(key_string);
    }
    rc = mdb_cursor_get(cursor, &key, &val, MDB_NEXT);
  }

  mdb_cursor_close(cursor);
  mdb_txn_abort(txn);

  if (rc != MDB_NOTFOUND) {
    logger_->log_error("Failed to iterate over LMDB database: {}", mdb_strerror(rc));
    return;
  }

  std::vector<std::string> failed_deletions;
  for (const auto& key : keys_to_be_deleted) {
    auto delete_result = removeKey(key);
    if (!delete_result) {
      logger_->log_warn("Failed to delete orphan resource {} from LMDB database", key);
      failed_deletions.push_back(key);
    }
  }

  std::lock_guard<std::mutex> lock(purge_list_mutex_);
  for (const auto& key : failed_deletions) {
    purge_list_.push_back(key);
  }
}

uint64_t LmdbContentRepository::getRepositorySize() const {
  MDB_stat stat;
  MDB_txn* txn = nullptr;
  mdb_txn_begin(lmdb_env_, nullptr, MDB_RDONLY, &txn);
  mdb_stat(txn, lmdb_handle_, &stat);
  mdb_txn_abort(txn);
  return  stat.ms_psize * (stat.ms_branch_pages + stat.ms_leaf_pages + stat.ms_overflow_pages);
}

uint64_t LmdbContentRepository::getRepositoryEntryCount() const {
  MDB_stat stat;
  MDB_txn* txn = nullptr;
  mdb_txn_begin(lmdb_env_, nullptr, MDB_RDONLY, &txn);
  mdb_stat(txn, lmdb_handle_, &stat);
  mdb_txn_abort(txn);
  return stat.ms_entries;
}

REGISTER_RESOURCE_AS(LmdbContentRepository, InternalResource, ("LmdbContentRepository", "lmdbcontentrepository"));

}  // namespace org::apache::nifi::minifi::core::repository
