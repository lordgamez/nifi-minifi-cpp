/**
 * @file S3Processor.cpp
 * Base S3 processor class implementation
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

#include "S3Processor.h"

#include <string>
#include <set>
#include <memory>

#include "S3Wrapper.h"
#include "AWSCredentialsService.h"
#include "properties/Properties.h"
#include "utils/StringUtils.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace aws {
namespace processors {

const std::set<std::string> S3Processor::REGIONS({region::AF_SOUTH_1, region::AP_EAST_1, region::AP_NORTHEAST_1,
  region::AP_NORTHEAST_2, region::AP_NORTHEAST_3, region::AP_SOUTH_1, region::AP_SOUTHEAST_1, region::AP_SOUTHEAST_2,
  region::CA_CENTRAL_1, region::CN_NORTH_1, region::CN_NORTHWEST_1, region::EU_CENTRAL_1, region::EU_NORTH_1,
  region::EU_SOUTH_1, region::EU_WEST_1, region::EU_WEST_2, region::EU_WEST_3, region::ME_SOUTH_1, region::SA_EAST_1,
  region::US_EAST_1, region::US_EAST_2, region::US_GOV_EAST_1, region::US_GOV_WEST_1, region::US_WEST_1, region::US_WEST_2});

const core::Property S3Processor::ObjectKey(
  core::PropertyBuilder::createProperty("Object Key")
    ->withDescription("The key of the S3 object. If none is given the filename attribute will be used by default.")
    ->supportsExpressionLanguage(true)
    ->build());
const core::Property S3Processor::Bucket(
  core::PropertyBuilder::createProperty("Bucket")
    ->withDescription("The S3 bucket")
    ->isRequired(true)
    ->supportsExpressionLanguage(true)
    ->build());
const core::Property S3Processor::AccessKey(
  core::PropertyBuilder::createProperty("Access Key")
    ->withDescription("AWS account access key")
    ->supportsExpressionLanguage(true)
    ->build());
const core::Property S3Processor::SecretKey(
  core::PropertyBuilder::createProperty("Secret Key")
    ->withDescription("AWS account secret key")
    ->supportsExpressionLanguage(true)
    ->build());
const core::Property S3Processor::CredentialsFile(
  core::PropertyBuilder::createProperty("Credentials File")
    ->withDescription("Path to a file containing AWS access key and secret key in properties file format. Properties used: accessKey and secretKey")
    ->build());
const core::Property S3Processor::AWSCredentialsProviderService(
  core::PropertyBuilder::createProperty("AWS Credentials Provider service")
    ->withDescription("The name of the AWS Credentials Provider controller service that is used to obtain AWS credentials.")
    ->build());
const core::Property S3Processor::Region(
  core::PropertyBuilder::createProperty("Region")
    ->isRequired(true)
    ->withDefaultValue<std::string>(region::US_WEST_2)
    ->withAllowableValues<std::string>(S3Processor::REGIONS)
    ->withDescription("AWS Region")
    ->build());
const core::Property S3Processor::CommunicationsTimeout(
  core::PropertyBuilder::createProperty("Communications Timeout")
    ->isRequired(true)
    ->withDefaultValue<core::TimePeriodValue>("30 sec")
    ->withDescription("")
    ->build());
const core::Property S3Processor::EndpointOverrideURL(
  core::PropertyBuilder::createProperty("Endpoint Override URL")
    ->withDescription("Endpoint URL to use instead of the AWS default including scheme, host, "
                      "port, and path. The AWS libraries select an endpoint URL based on the AWS "
                      "region, but this property overrides the selected endpoint URL, allowing use "
                      "with other S3-compatible endpoints.")
    ->supportsExpressionLanguage(true)
    ->build());
const core::Property S3Processor::ProxyHost(
  core::PropertyBuilder::createProperty("Proxy Host")
    ->withDescription("Proxy host name or IP")
    ->supportsExpressionLanguage(true)
    ->build());
const core::Property S3Processor::ProxyPort(
  core::PropertyBuilder::createProperty("Proxy Port")
    ->withDescription("The port number of the proxy host")
    ->supportsExpressionLanguage(true)
    ->build());
const core::Property S3Processor::ProxyUsername(
    core::PropertyBuilder::createProperty("Proxy Username")
    ->withDescription("Username to set when authenticating against proxy")
    ->supportsExpressionLanguage(true)
    ->build());
const core::Property S3Processor::ProxyPassword(
  core::PropertyBuilder::createProperty("Proxy Password")
    ->withDescription("Password to set when authenticating against proxy")
    ->supportsExpressionLanguage(true)
    ->build());
const core::Property S3Processor::UseDefaultCredentials(
    core::PropertyBuilder::createProperty("Use Default Credentials")
    ->withDescription("If true, uses the Default Credential chain, including EC2 instance profiles or roles, environment variables, default user credentials, etc.")
    ->withDefaultValue<bool>(false)
    ->isRequired(true)
    ->build());

S3Processor::S3Processor(std::string name, minifi::utils::Identifier uuid, const std::shared_ptr<logging::Logger> &logger)
  : core::Processor(std::move(name), uuid)
  , logger_(logger)
  , s3_wrapper_(minifi::utils::make_unique<aws::s3::S3Wrapper>()) {
}

S3Processor::S3Processor(std::string name, minifi::utils::Identifier uuid, const std::shared_ptr<logging::Logger> &logger, std::unique_ptr<aws::s3::S3WrapperBase> s3_wrapper)
  : core::Processor(std::move(name), uuid)
  , logger_(logger)
  , s3_wrapper_(std::move(s3_wrapper)) {
}

const std::set<core::Property> S3Processor::getSupportedProperties() {
  return {ObjectKey, Bucket, AccessKey, SecretKey, CredentialsFile, CredentialsFile, AWSCredentialsProviderService, Region, CommunicationsTimeout,
    EndpointOverrideURL, ProxyHost, ProxyPort, ProxyUsername, ProxyPassword, UseDefaultCredentials};
}

minifi::utils::optional<Aws::Auth::AWSCredentials> S3Processor::getAWSCredentialsFromControllerService(const std::shared_ptr<core::ProcessContext> &context) const {
  std::string service_name;
  if (!context->getProperty(AWSCredentialsProviderService.getName(), service_name) || service_name.empty()) {
    return minifi::utils::nullopt;
  }

  std::shared_ptr<core::controller::ControllerService> service = context->getControllerService(service_name);
  if (!service) {
    return minifi::utils::nullopt;
  }

  auto aws_credentials_service = std::dynamic_pointer_cast<minifi::aws::controllers::AWSCredentialsService>(service);
  if (!aws_credentials_service) {
    return minifi::utils::nullopt;
  }

  return minifi::utils::make_optional<Aws::Auth::AWSCredentials>(aws_credentials_service->getAWSCredentials());
}

minifi::utils::optional<Aws::Auth::AWSCredentials> S3Processor::getAWSCredentials(
    const std::shared_ptr<core::ProcessContext> &context,
    const std::shared_ptr<core::FlowFile> &flow_file) {
  auto service_cred = getAWSCredentialsFromControllerService(context);
  if (service_cred) {
    logger_->log_info("AWS Credentials successfully set from controller service");
    return service_cred.value();
  }

  std::string access_key;
  context->getProperty(AccessKey, access_key, flow_file);
  aws_credentials_provider_.setAccessKey(access_key);
  std::string secret_key;
  context->getProperty(SecretKey, secret_key, flow_file);
  aws_credentials_provider_.setSecretKey(secret_key);
  std::string credential_file;
  context->getProperty(CredentialsFile.getName(), credential_file);
  aws_credentials_provider_.setCredentialsFile(credential_file);
  bool use_default_credentials = false;
  context->getProperty(UseDefaultCredentials.getName(), use_default_credentials);
  aws_credentials_provider_.setUseDefaultCredentials(use_default_credentials);

  return aws_credentials_provider_.getAWSCredentials();
}

bool S3Processor::setProxy(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::FlowFile> &flow_file) {
  aws::s3::ProxyOptions proxy;
  context->getProperty(ProxyHost, proxy.host, flow_file);
  std::string port_str;
  if (context->getProperty(ProxyPort, port_str, flow_file) && !port_str.empty() && !core::Property::StringToInt(port_str, proxy.port)) {
    logger_->log_error("Proxy port invalid");
    return false;
  }
  context->getProperty(ProxyUsername, proxy.username, flow_file);
  context->getProperty(ProxyPassword, proxy.password, flow_file);
  if (!proxy.host.empty()) {
    s3_wrapper_->setProxy(proxy);
    logger_->log_info("Proxy for S3Processor was set.");
  }
  return true;
}

void S3Processor::onSchedule(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::ProcessSessionFactory> &sessionFactory) {
  if (!context->getProperty(Bucket.getName(), bucket_) || bucket_.empty()) {
    throw Exception(PROCESS_SCHEDULE_EXCEPTION, "Bucket property missing or invalid");
  }
  logger_->log_debug("S3Processor: Bucket [%s]", bucket_);

  std::string value;
  if (!context->getProperty(Region.getName(), value) || value.empty() || REGIONS.count(value) == 0) {
    throw Exception(PROCESS_SCHEDULE_EXCEPTION, "Region property missing or invalid");
  }
  s3_wrapper_->setRegion(value);
  logger_->log_debug("S3Processor: Region [%s]", value);

  uint64_t timeout_val;
  if (context->getProperty(CommunicationsTimeout.getName(), value) && !value.empty() && core::Property::getTimeMSFromString(value, timeout_val)) {
    s3_wrapper_->setTimeout(timeout_val);
    logger_->log_debug("S3Processor: Communications Timeout [%d]", timeout_val);
  } else {
    throw Exception(PROCESS_SCHEDULE_EXCEPTION, "Communications Timeout missing or invalid");
  }
}

bool S3Processor::getExpressionLanguageSupportedProperties(
    const std::shared_ptr<core::ProcessContext> &context,
    const std::shared_ptr<core::FlowFile> &flow_file) {
  context->getProperty(ObjectKey, object_key_, flow_file);
  if (object_key_.empty() && (!flow_file->getAttribute("filename", object_key_) || object_key_.empty())) {
    logger_->log_error("No Object Key is set and default object key 'filename' attribute could not be found!");
    return false;
  }
  logger_->log_debug("S3Processor: Object Key [%s]", object_key_);

  if (!context->getProperty(Bucket, bucket_, flow_file) || bucket_.empty()) {
    logger_->log_error("Bucket is invalid or empty!", bucket_);
    return false;
  }
  logger_->log_debug("S3Processor: Bucket [%s]", bucket_);

  auto credentials = getAWSCredentials(context, flow_file);
  if (!credentials) {
    logger_->log_error("AWS Credentials have not been set!");
    return false;
  }
  s3_wrapper_->setCredentials(credentials.value());

  if (!setProxy(context, flow_file)) {
    return false;
  }

  std::string value;
  if (context->getProperty(EndpointOverrideURL, value, flow_file) && !value.empty()) {
    s3_wrapper_->setEndpointOverrideUrl(value);
    logger_->log_debug("S3Processor: Endpoint Override URL [%d]", value);
  }

  return true;
}

}  // namespace processors
}  // namespace aws
}  // namespace minifi
}  // namespace nifi
}  // namespace apache
}  // namespace org
