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

bool LmdbContentRepository::initialize(const std::shared_ptr<minifi::Configure> &configuration) {
  MDB_env *env;
  if (const int rc = mdb_env_create(&env)) {
    logger_->log_error("Failed to create LMDB environment: {}", mdb_strerror(rc));
    return false;
  }
  // //Limit large enough to accommodate all our named dbs. This only starts to matter if the number gets large, otherwise it's just a bunch of extra entries in the main table.
  mdb_env_set_maxdbs(env, 4);

  //This is the maximum size of the db (but will not be used directly), so we make it large enough that we hopefully never run into the limit.
  mdb_env_set_mapsize(env, (size_t)1048576 * (size_t)100000); // 1MB * 100000

  const auto working_dir = utils::getMinifiDir();

  std::string value;
  if (configuration->get(Configure::nifi_dbcontent_repository_directory_default, value) && !value.empty()) {
    directory_ = value;
  } else {
    directory_ = (working_dir / "dbcontentrepository").string();
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
  return nullptr;
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
