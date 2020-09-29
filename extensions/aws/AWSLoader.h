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

#pragma once

#include <vector>
#include <string>
#include <memory>

#include "core/ClassLoader.h"
#include "utils/StringUtils.h"
#include "controllerservices/AWSCredentialsService.h"
#include "processors/PutS3Object.h"

class AWSObjectFactory : public core::ObjectFactory {
 public:
  AWSObjectFactory() = default;

  /**
   * Gets the name of the object.
   * @return class name of processor
   */
  std::string getName() override {
    return "AWSObjectFactory";
  }

  std::string getClassName() override {
    return "AWSObjectFactory";
  }

  /**
   * Gets the class name for the object
   * @return class name for the processor.
   */
  std::vector<std::string> getClassNames() override {
    std::vector<std::string> class_names;
    class_names.push_back("AWSCredentialsService");
    return class_names;
  }

  std::unique_ptr<ObjectFactory> assign(const std::string &class_name) override {
    if (utils::StringUtils::equalsIgnoreCase(class_name, "AWSCredentialsService")) {
      return std::unique_ptr<ObjectFactory>(new core::DefautObjectFactory<minifi::aws::controllers::AWSCredentialsService>());
    } else if (utils::StringUtils::equalsIgnoreCase(class_name, "PutS3Object")) {
      return std::unique_ptr<ObjectFactory>(new core::DefautObjectFactory<minifi::aws::processors::PutS3Object>());
    } else {
      return nullptr;
    }
  }

  static bool added;
};

extern "C" {
DLL_EXPORT void *createAWSFactory(void);
}
