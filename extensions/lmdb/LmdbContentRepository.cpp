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

#include "minifi-cpp/Exception.h"
#include "core/Resource.h"
#include "core/TypedValues.h"
#include "utils/Locations.h"
#include "minifi-cpp/utils/gsl.h"

namespace org::apache::nifi::minifi::core::repository {

bool LmdbContentRepository::initialize(const std::shared_ptr<minifi::Configure> &configuration) {
  const auto working_dir = utils::getMinifiDir();

  std::string value;
  if (configuration->get(Configure::nifi_dbcontent_repository_directory_default, value) && !value.empty()) {
    directory_ = value;
  } else {
    directory_ = (working_dir / "dbcontentrepository").string();
  }
  auto purge_period_str = utils::string::trim(configuration->get(Configure::nifi_dbcontent_repository_purge_period).value_or("1 s"));
  if (purge_period_str == "0") {
    purge_period_ = std::chrono::seconds{0};
  } else if (auto purge_period_val = core::TimePeriodValue::fromString(purge_period_str)) {
    purge_period_ = purge_period_val->getMilliseconds();
  } else {
    logger_->log_error("Malformed delete period value, expected time format: '{}'", purge_period_str);
    purge_period_ = std::chrono::seconds{1};
  }
  const auto encrypted_env = createEncryptingEnv(utils::crypto::EncryptionManager{working_dir}, DbEncryptionOptions{directory_, ENCRYPTION_KEY_NAME});
  logger_->log_info("Using {} LmdbContentRepository", encrypted_env ? "encrypted" : "plaintext");

  setCompactionPeriod(configuration);

  auto set_db_opts = [encrypted_env] (minifi::internal::Writable<rocksdb::DBOptions>& db_opts) {
    minifi::internal::setCommonRocksDbOptions(db_opts);
    if (encrypted_env) {
      db_opts.set(&rocksdb::DBOptions::env, encrypted_env.get(), EncryptionEq{});
    } else {
      db_opts.set(&rocksdb::DBOptions::env, rocksdb::Env::Default());
    }
  };
  auto set_cf_opts = [&configuration] (rocksdb::ColumnFamilyOptions& cf_opts) {
    cf_opts.OptimizeForPointLookup(4);
    cf_opts.merge_operator = std::make_shared<StringAppender>();
    cf_opts.max_successive_merges = 0;
    if (auto compression_type = minifi::internal::readConfiguredCompressionType(configuration, Configure::nifi_content_repository_rocksdb_compression)) {
      cf_opts.compression = *compression_type;
    }
  };
  db_ = minifi::internal::RocksDatabase::create(set_db_opts, set_cf_opts, directory_,
    minifi::internal::getRocksDbOptionsToOverride(configuration, Configure::nifi_content_repository_rocksdb_options));
  if (db_->open()) {
    logger_->log_debug("NiFi Content DB Repository database open {} success", directory_);
    is_valid_ = true;
  } else {
    logger_->log_error("NiFi Content DB Repository database open {} fail", directory_);
    is_valid_ = false;
  }

  use_synchronous_writes_ = configuration->get(Configure::nifi_content_repository_rocksdb_use_synchronous_writes).value_or("true") != "false";
  verify_checksums_in_rocksdb_reads_ = (configuration->get(Configure::nifi_content_repository_rocksdb_read_verify_checksums) | utils::andThen(&utils::string::toBool)).value_or(false);
  logger_->log_debug("{} checksum verification in LmdbContentRepository", verify_checksums_in_rocksdb_reads_ ? "Using" : "Not using");
  return is_valid_;
}

void LmdbContentRepository::start() {
  if (!db_ || !is_valid_) {
    return;
  }
  if (compaction_period_.count() != 0) {
    compaction_thread_ = std::make_unique<utils::StoppableThread>([this] {
      runCompaction();
    });
  }
  if (purge_period_.count() != 0) {
    gc_thread_ = std::make_unique<utils::StoppableThread>([this] {
      runGc();
    });
  }
}

void LmdbContentRepository::stop() {
  if (db_) {
    auto opendb = db_->open();
    if (opendb) {
      opendb->FlushWAL(true);
    }
    compaction_thread_.reset();
    gc_thread_.reset();
  }
}

LmdbContentRepository::Session::Session(std::shared_ptr<ContentRepository> repository, bool use_synchronous_writes)
    : BufferedContentSession(std::move(repository)),
      use_synchronous_writes_(use_synchronous_writes) {}

std::shared_ptr<ContentSession> LmdbContentRepository::createSession() {
  return std::make_shared<Session>(sharedFromThis<ContentRepository>(), use_synchronous_writes_);
}

void LmdbContentRepository::Session::commit() {
  auto dbContentRepository = std::dynamic_pointer_cast<LmdbContentRepository>(repository_);
  auto opendb = dbContentRepository->db_->open();
  if (!opendb) {
    throw Exception(REPOSITORY_EXCEPTION, "Couldn't open rocksdb database to commit content changes");
  }
  auto batch = opendb->createWriteBatch();
  for (const auto& resource : managed_resources_) {
    auto outStream = dbContentRepository->write(*resource.first, false, &batch);
    if (outStream == nullptr) {
      throw Exception(REPOSITORY_EXCEPTION, "Couldn't open the underlying resource for write: " + resource.first->getContentFullPath());
    }
    const auto size = resource.second->size();
    if (outStream->write(resource.second->getBuffer()) != size) {
      throw Exception(REPOSITORY_EXCEPTION, "Failed to write new resource: " + resource.first->getContentFullPath());
    }
  }
  for (const auto& resource : append_state_) {
    auto outStream = dbContentRepository->write(*resource.first, true, &batch);
    if (outStream == nullptr) {
      throw Exception(REPOSITORY_EXCEPTION, "Couldn't open the underlying resource for append: " + resource.first->getContentFullPath());
    }
    const auto size = resource.second.stream->size();
    if (outStream->write(resource.second.stream->getBuffer()) != size) {
      throw Exception(REPOSITORY_EXCEPTION, "Failed to append to resource: " + resource.first->getContentFullPath());
    }
  }

  rocksdb::WriteOptions options;
  options.sync = use_synchronous_writes_;
  rocksdb::Status status = opendb->Write(options, &batch);
  if (!status.ok()) {
    throw Exception(REPOSITORY_EXCEPTION, "Batch write failed: " + status.ToString());
  }

  managed_resources_.clear();
  append_state_.clear();
}

std::shared_ptr<io::BaseStream> LmdbContentRepository::write(const minifi::ResourceClaim &claim, bool append) {
  return write(claim, append, nullptr);
}

std::shared_ptr<io::BaseStream> LmdbContentRepository::read(const minifi::ResourceClaim &claim) {
  // the traditional approach with these has been to return -1 from the stream; however, since we have the ability here
  // we can simply return a nullptr, which is also valid from the API when this stream is not valid.
  if (!is_valid_ || !db_)
    return nullptr;
  return std::make_shared<io::RocksDbStream>(claim.getContentFullPath(), gsl::make_not_null<minifi::internal::RocksDatabase*>(db_.get()), false, nullptr, true, verify_checksums_in_rocksdb_reads_);
}

bool LmdbContentRepository::exists(const minifi::ResourceClaim &streamId) {
  auto opendb = db_->open();
  if (!opendb) {
    return false;
  }
  std::string value;
  rocksdb::Status status;
  rocksdb::ReadOptions options;
  options.verify_checksums = verify_checksums_in_rocksdb_reads_;
  status = opendb->Get(options, streamId.getContentFullPath(), &value);
  if (status.ok()) {
    logger_->log_debug("{} exists", streamId.getContentFullPath());
    return true;
  } else {
    logger_->log_debug("{} does not exist", streamId.getContentFullPath());
    return false;
  }
}

bool LmdbContentRepository::removeKey(const std::string& content_path) {
  if (purge_period_ == std::chrono::seconds(0)) {
    return removeKeySync(content_path);
  }
  // asynchronous deletion
  std::lock_guard guard(keys_mtx_);
  logger_->log_debug("Staging resource for deletion {}", content_path);
  keys_to_delete_.push_back(content_path);
  return true;
}

std::shared_ptr<io::BaseStream> LmdbContentRepository::write(const minifi::ResourceClaim& claim, bool /*append*/, minifi::internal::WriteBatch* batch) {
  // the traditional approach with these has been to return -1 from the stream; however, since we have the ability here
  // we can simply return a nullptr, which is also valid from the API when this stream is not valid.
  if (!is_valid_ || !db_)
    return nullptr;
  // append is already supported in all modes
  return std::make_shared<io::RocksDbStream>(claim.getContentFullPath(), gsl::make_not_null<minifi::internal::RocksDatabase*>(db_.get()), true, batch, true, verify_checksums_in_rocksdb_reads_);
}

void LmdbContentRepository::clearOrphans() {
  if (!is_valid_ || !db_) {
    logger_->log_error("Cannot delete orphan content entries, repository is invalid");
    return;
  }
  auto opendb = db_->open();
  if (!opendb) {
    logger_->log_error("Cannot delete orphan content entries, could not open repository");
    return;
  }
  std::vector<std::string> keys_to_be_deleted;
  rocksdb::ReadOptions options;
  options.verify_checksums = verify_checksums_in_rocksdb_reads_;
  auto it = opendb->NewIterator(options);
  for (it->SeekToFirst(); it->Valid(); it->Next()) {
    auto key = it->key().ToString();
    std::lock_guard<std::mutex> lock(count_map_mutex_);
    auto claim_it = count_map_.find(key);
    if (claim_it == count_map_.end() || claim_it->second == 0) {
      logger_->log_error("Deleting orphan resource {}", key);
      keys_to_be_deleted.push_back(key);
    }
  }
  auto batch = opendb->createWriteBatch();
  for (auto& key : keys_to_be_deleted) {
    batch.Delete(key);
  }

  rocksdb::Status status = opendb->Write(rocksdb::WriteOptions(), &batch);

  if (!status.ok()) {
    logger_->log_error("Could not delete orphan contents from rocksdb database: {}", status.ToString());
    std::lock_guard<std::mutex> lock(purge_list_mutex_);
    for (const auto& key : keys_to_be_deleted) {
      purge_list_.push_back(key);
    }
  }
}

uint64_t LmdbContentRepository::getRepositorySize() const {
  return (utils::optional_from_ptr(db_.get()) |
          utils::andThen([](const auto& db) { return db->open(); }) |
          utils::andThen([](const auto& opendb) { return opendb.getApproximateSizes(); })).value_or(0);
}

uint64_t LmdbContentRepository::getRepositoryEntryCount() const {
  return (utils::optional_from_ptr(db_.get()) |
          utils::andThen([](const auto& db) { return db->open(); }) |
          utils::andThen([](auto&& opendb) -> std::optional<uint64_t> {
              std::string key_count;
              opendb.GetProperty("rocksdb.estimate-num-keys", &key_count);
              if (!key_count.empty()) {
                return std::stoull(key_count);
              }
              return std::nullopt;
            })).value_or(0);
}

REGISTER_RESOURCE_AS(LmdbContentRepository, InternalResource, ("LmdbContentRepository", "LmdbContentRepository"));

}  // namespace org::apache::nifi::minifi::core::repository
