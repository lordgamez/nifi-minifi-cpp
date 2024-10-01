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

#include "CouchbaseClusterService.h"

#include "core/Resource.h"

namespace org::apache::nifi::minifi::couchbase::controllers {

void CouchbaseClusterService::initialize() {
  setSupportedProperties(Properties);
}

void CouchbaseClusterService::onEnable() {
  std::string connection_string;
  getProperty(ConnectionString, connection_string);
  std::string username;
  getProperty(UserName, username);
  std::string password;
  getProperty(UserPassword, password);

  // TODO: add support for SSL through linked services
  // std::shared_ptr<minifi::controllers::SSLContextService> ssl_context_service;
  // if (auto ssl_context = context.getProperty(SSLContextService)) {
  //   ssl_context_service = std::dynamic_pointer_cast<minifi::controllers::SSLContextService>(context.getControllerService(*ssl_context));
  // }

  // ::couchbase::cluster::options options;
  // if (ssl_context_service) {
  //   options.tls_config.certificate_path = ssl_context_service->getCertificateFile();
  //   options.tls_config.private_key_path = ssl_context_service->getPrivateKeyFile();
  //   options.tls_config.ca_cert_path = ssl_context_service->getCACertificate();
  // } else {
  //   options = ::couchbase::cluster_options(username, password);
  // }

  auto options = ::couchbase::cluster_options(username, password);
  auto [connect_err, cluster] = ::couchbase::cluster::connect(connection_string, options).get();
  if (connect_err.ec()) {
    logger_->log_error("Failed to connect to Couchbase cluster: {}", connect_err.message());
    throw Exception(CONTROLLER_ENABLE_EXCEPTION, "Failed to connect to Couchbase cluster: " + connect_err.ec().message());
  }
  cluster_ = std::move(cluster);
}

gsl::not_null<std::shared_ptr<CouchbaseClusterService>> CouchbaseClusterService::getFromProperty(const core::ProcessContext& context, const core::PropertyReference& property) {
  std::shared_ptr<CouchbaseClusterService> couchbase_cluster_service;
  if (auto connection_controller_name = context.getProperty(property)) {
    couchbase_cluster_service = std::dynamic_pointer_cast<CouchbaseClusterService>(context.getControllerService(*connection_controller_name));
  }
  if (!couchbase_cluster_service) {
    throw minifi::Exception(ExceptionType::PROCESS_SCHEDULE_EXCEPTION, "Missing Couchbase Cluster Service");
  }
  return gsl::make_not_null(couchbase_cluster_service);
}

REGISTER_RESOURCE(CouchbaseClusterService, ControllerService);

}  // namespace org::apache::nifi::minifi::couchbase::controllers
