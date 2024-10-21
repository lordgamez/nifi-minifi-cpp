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
#include <utility>
#include <string>
#include "CouchbaseClusterService.h"
#include "unit/Catch.h"

using namespace std::literals::chrono_literals;

namespace org::apache::nifi::minifi::couchbase::test {

const std::uint64_t COUCHBASE_PUT_RESULT_CAS = 9876;
const std::uint64_t COUCHBASE_PUT_RESULT_SEQUENCE_NUMBER = 345;
const std::uint64_t COUCHBASE_PUT_RESULT_PARTITION_UUID = 7890123456;
const std::uint16_t COUCHBASE_PUT_RESULT_PARTITION_ID = 1234;

struct UpsertParameters {
  CouchbaseValueType document_type;
  std::string document_id;
  std::vector<std::byte> buffer;
  ::couchbase::upsert_options options;
};

class MockCouchbaseClusterService : public controllers::CouchbaseClusterService {
 public:
  using CouchbaseClusterService::CouchbaseClusterService;
  EXTENSIONAPI static constexpr bool SupportsDynamicProperties = false;
  ADD_COMMON_VIRTUAL_FUNCTIONS_FOR_CONTROLLER_SERVICES

  void onEnable() override {}
  void notifyStop() override {}

  nonstd::expected<CouchbaseUpsertResult, CouchbaseErrorType> upsert(const CouchbaseCollection& collection, CouchbaseValueType document_type, const std::string& document_id,
      const std::vector<std::byte>& buffer, const ::couchbase::upsert_options& options) override {
    collection_ = collection;
    upsert_parameters_.document_type = document_type;
    upsert_parameters_.document_id = document_id;
    upsert_parameters_.buffer = buffer;
    upsert_parameters_.options = options;

    if (upsert_error_) {
      return nonstd::make_unexpected(*upsert_error_);
    } else {
      return CouchbaseUpsertResult{collection_.bucket_name, COUCHBASE_PUT_RESULT_CAS, COUCHBASE_PUT_RESULT_SEQUENCE_NUMBER, COUCHBASE_PUT_RESULT_PARTITION_UUID, COUCHBASE_PUT_RESULT_PARTITION_ID};
    }
  }

  UpsertParameters getUpsertParameters() const {
    return upsert_parameters_;
  }

  CouchbaseCollection getCollectionParameter() const {
    return collection_;
  }

  void setUpsertError(const CouchbaseErrorType upsert_error) {
    upsert_error_ = upsert_error;
  }

 private:
  CouchbaseCollection collection_;
  UpsertParameters upsert_parameters_;
  std::optional<CouchbaseErrorType> upsert_error_;
};
}  // namespace org::apache::nifi::minifi::couchbase::test