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
#include "lmdb.h"

namespace org::apache::nifi::minifi::core::repository {

LmdbContentRepository::Session::Session(std::shared_ptr<ContentRepository> repository)
    : BufferedContentSession(std::move(repository)) {}

void LmdbContentRepository::Session::commit() {
  auto lmdbContentRepository = std::dynamic_pointer_cast<LmdbContentRepository>(repository_);
  if (!lmdbContentRepository) {
    throw Exception(REPOSITORY_EXCEPTION, "Session's repository is not an LmdbContentRepository");
  }
  // TODO: do write first

  // auto opendb = lmdbContentRepository->db_->open();
  // if (!opendb) {
  //   throw Exception(REPOSITORY_EXCEPTION, "Couldn't open rocksdb database to commit content changes");
  // }
  // auto batch = opendb->createWriteBatch();
  // for (const auto& resource : managed_resources_) {
  //   auto outStream = dbContentRepository->write(*resource.first, false, &batch);
  //   if (outStream == nullptr) {
  //     throw Exception(REPOSITORY_EXCEPTION, "Couldn't open the underlying resource for write: " + resource.first->getContentFullPath());
  //   }
  //   const auto size = resource.second->size();
  //   if (outStream->write(resource.second->getBuffer()) != size) {
  //     throw Exception(REPOSITORY_EXCEPTION, "Failed to write new resource: " + resource.first->getContentFullPath());
  //   }
  // }
  // for (const auto& resource : append_state_) {
  //   auto outStream = dbContentRepository->write(*resource.first, true, &batch);
  //   if (outStream == nullptr) {
  //     throw Exception(REPOSITORY_EXCEPTION, "Couldn't open the underlying resource for append: " + resource.first->getContentFullPath());
  //   }
  //   const auto size = resource.second.stream->size();
  //   if (outStream->write(resource.second.stream->getBuffer()) != size) {
  //     throw Exception(REPOSITORY_EXCEPTION, "Failed to append to resource: " + resource.first->getContentFullPath());
  //   }
  // }

  // rocksdb::WriteOptions options;
  // options.sync = use_synchronous_writes_;
  // rocksdb::Status status = opendb->Write(options, &batch);
  // if (!status.ok()) {
  //   throw Exception(REPOSITORY_EXCEPTION, "Batch write failed: " + status.ToString());
  // }

  // managed_resources_.clear();
  // append_state_.clear();
}

bool LmdbContentRepository::initialize(const std::shared_ptr<minifi::Configure> &configuration) {
  MDB_env *env;
  if (const int rc = mdb_env_create(&env)) {
    logger_->log_error("Failed to create LMDB environment: {}", mdb_strerror(rc));
    return false;
  }
  //Limit large enough to accommodate all our named dbs. This only starts to matter if the number gets large, otherwise it's just a bunch of extra entries in the main table.
  mdb_env_set_maxdbs(env, 4);

  //This is the maximum size of the db (but will not be used directly), so we make it large enough that we hopefully never run into the limit.
  mdb_env_set_mapsize(env, (size_t)1048576 * (size_t)100000); // 1MB * 100000

  const auto working_dir = utils::getMinifiDir();

  std::string value;
  if (configuration->get(Configure::nifi_dbcontent_repository_directory_default, value) && !value.empty()) {
    directory_ = value;
  } else {
    directory_ = (working_dir / "lmdbcontentrepository").string();
  }
  if (const int rc = mdb_env_open(env, directory_.c_str(), MDB_NOTLS | MDB_RDONLY, 0664)) {
    logger_->log_error("Failed to open LMDB environment: {}", mdb_strerror(rc));
    return false;
  }
  return true;
}

void LmdbContentRepository::start() {
  // add compaction thread if needed
}

void LmdbContentRepository::stop() {

}

std::shared_ptr<ContentSession> LmdbContentRepository::createSession() {
  return std::make_shared<Session>(sharedFromThis<ContentRepository>());
}

std::shared_ptr<io::BaseStream> LmdbContentRepository::write(const minifi::ResourceClaim &claim, bool append) {
  return nullptr;
}

std::shared_ptr<io::BaseStream> LmdbContentRepository::read(const minifi::ResourceClaim &claim) {
  return nullptr;
}

bool LmdbContentRepository::exists(const minifi::ResourceClaim &streamId) {
  return false;
}

bool LmdbContentRepository::removeKey(const std::string& content_path) {
  return false;
}

void LmdbContentRepository::clearOrphans() {

}

uint64_t LmdbContentRepository::getRepositorySize() const {
  return 0;
}

uint64_t LmdbContentRepository::getRepositoryEntryCount() const {
  return 0;
}

REGISTER_RESOURCE_AS(LmdbContentRepository, InternalResource, ("LmdbContentRepository", "LmdbContentRepository"));

}  // namespace org::apache::nifi::minifi::core::repository
