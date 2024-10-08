/**
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

#include "GetCouchbaseKey.h"
#include "utils/gsl.h"
#include "core/Resource.h"

namespace org::apache::nifi::minifi::couchbase::processors {

void GetCouchbaseKey::onSchedule(core::ProcessContext& context, core::ProcessSessionFactory&) {
  couchbase_cluster_service_ = controllers::CouchbaseClusterService::getFromProperty(context, GetCouchbaseKey::CouchbaseClusterControllerService);
}

void GetCouchbaseKey::onTrigger(core::ProcessContext& context, core::ProcessSession& session) {
  gsl_Expects(couchbase_cluster_service_);

  auto flow_file = session.get();
  if (!flow_file) {
    context.yield();
    return;
  }

  CouchbaseCollection collection;
  if (!context.getProperty(BucketName, collection.bucket_name, flow_file.get()) || collection.bucket_name.empty()) {
    logger_->log_error("Bucket '{}' is invalid or empty!", collection.bucket_name);
    session.transfer(flow_file, Failure);
    return;
  }

  if (!context.getProperty(ScopeName, collection.scope_name, flow_file.get()) || collection.scope_name.empty()) {
    collection.scope_name = ::couchbase::scope::default_name;
  }

  if (!context.getProperty(CollectionName, collection.collection_name, flow_file.get()) || collection.collection_name.empty()) {
    collection.collection_name = ::couchbase::collection::default_name;
  }

  std::string document_id;
  if (!context.getProperty(DocumentId, document_id, flow_file.get()) || document_id.empty()) {
    auto ff_content = session.readBuffer(flow_file).buffer;
    document_id = std::string(reinterpret_cast<const char*>(ff_content.data()), ff_content.size());
  }

  if (document_id.empty()) {
    logger_->log_error("Document ID is empty, transferring FlowFile to failure relationship");
    session.transfer(flow_file, Failure);
    return;
  }

  std::string attribute_to_put_result_to;
  context.getProperty(PutValueToAttribute, attribute_to_put_result_to, flow_file.get());

  nonstd::expected<CouchbaseGetResult, CouchbaseErrorType> get_result;
  if (attribute_to_put_result_to.empty()) {
    get_result = couchbase_cluster_service_->get(collection, document_id, CouchbaseValueType::BINARY);
  } else {
    get_result = couchbase_cluster_service_->get(collection, document_id, CouchbaseValueType::STRING);
  }

  if (get_result) {
    if (!attribute_to_put_result_to.empty()) {
      session.putAttribute(*flow_file, attribute_to_put_result_to, std::get<std::string>(get_result->value));
    } else {
      session.write(flow_file, [&, this](const std::shared_ptr<io::OutputStream>& stream) -> int64_t {
        auto& value = std::get<std::vector<std::byte>>(get_result->value);
        stream->write(value);
        return gsl::narrow<int64_t>(value.size());
      });
    }

    session.putAttribute(*flow_file, "couchbase.bucket", get_result->bucket_name);
    session.putAttribute(*flow_file, "couchbase.doc.id", document_id);
    session.putAttribute(*flow_file, "couchbase.doc.cas", std::to_string(get_result->cas));
    session.transfer(flow_file, Success);
  } else if (get_result.error() == CouchbaseErrorType::TEMPORARY) {
    logger_->log_error("Failed to get document '{}' from collection '{}.{}.{}' due to timeout, transferring to retry relationship",
      document_id, collection.bucket_name, collection.scope_name, collection.collection_name);
    session.transfer(flow_file, Retry);
  } else {
    logger_->log_error("Failed to get document '{}' from collection '{}.{}.{}', transferring to failure relationship",
      document_id, collection.bucket_name, collection.scope_name, collection.collection_name);
    session.transfer(flow_file, Failure);
  }
}

REGISTER_RESOURCE(GetCouchbaseKey, Processor);

}  // namespace org::apache::nifi::minifi::couchbase::processors
