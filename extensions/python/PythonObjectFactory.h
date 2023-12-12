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

#include <map>
#include <vector>
#include <string>
#include <memory>
#include <utility>
#include <filesystem>

#include "core/ClassLoader.h"
#include "ExecutePythonProcessor.h"
#include "utils/StringUtils.h"

#if defined(__GNUC__) || defined(__GNUG__)
#pragma GCC visibility push(hidden)
#endif

enum class PythonProcessorType {
  MINIFI_TYPE,
  NIFI_TYPE
};

class PythonObjectFactory : public org::apache::nifi::minifi::core::DefautObjectFactory<org::apache::nifi::minifi::extensions::python::processors::ExecutePythonProcessor> {
 public:
  explicit PythonObjectFactory(std::string file, std::string name, PythonProcessorType python_processor_type, const std::vector<std::filesystem::path>& python_paths)
      : file_(std::move(file)),
        name_(std::move(name)),
        python_paths_(python_paths),
        python_processor_type_(python_processor_type) {
  }

  std::unique_ptr<org::apache::nifi::minifi::core::CoreComponent> create(const std::string &name) override {
    auto obj = DefautObjectFactory::create(name);
    auto ptr = org::apache::nifi::minifi::utils::dynamic_unique_cast<org::apache::nifi::minifi::extensions::python::processors::ExecutePythonProcessor>(std::move(obj));
    if (ptr == nullptr) {
      return nullptr;
    }
    if (python_processor_type_ == PythonProcessorType::NIFI_TYPE) {
      auto name_tokens = org::apache::nifi::minifi::utils::StringUtils::split(name_, ".");
      ptr->setPythonClassName(name_tokens[name_tokens.size() - 1]);
      ptr->setPythonPaths(python_paths_);
    }
    ptr->initialize();
    ptr->setProperty(org::apache::nifi::minifi::extensions::python::processors::ExecutePythonProcessor::ScriptFile, file_);
    return ptr;
  }

  std::unique_ptr<org::apache::nifi::minifi::core::CoreComponent> create(const std::string &name, const org::apache::nifi::minifi::utils::Identifier &uuid) override {
    auto obj = DefautObjectFactory::create(name, uuid);
    auto ptr = org::apache::nifi::minifi::utils::dynamic_unique_cast<org::apache::nifi::minifi::extensions::python::processors::ExecutePythonProcessor>(std::move(obj));
    if (ptr == nullptr) {
      return nullptr;
    }
    if (python_processor_type_ == PythonProcessorType::NIFI_TYPE) {
      auto name_tokens = org::apache::nifi::minifi::utils::StringUtils::split(name_, ".");
      ptr->setPythonClassName(name_tokens[name_tokens.size() - 1]);
      ptr->setPythonPaths(python_paths_);
    }
    ptr->initialize();
    ptr->setProperty(org::apache::nifi::minifi::extensions::python::processors::ExecutePythonProcessor::ScriptFile, file_);
    return ptr;
  }

  org::apache::nifi::minifi::core::CoreComponent* createRaw(const std::string &name) override {
    auto ptr = dynamic_cast<org::apache::nifi::minifi::extensions::python::processors::ExecutePythonProcessor*>(DefautObjectFactory::createRaw(name));
    ptr->initialize();
    ptr->setProperty(org::apache::nifi::minifi::extensions::python::processors::ExecutePythonProcessor::ScriptFile, file_);
    return dynamic_cast<org::apache::nifi::minifi::core::CoreComponent*>(ptr);
  }

  org::apache::nifi::minifi::core::CoreComponent* createRaw(const std::string &name, const org::apache::nifi::minifi::utils::Identifier &uuid) override {
    auto ptr = dynamic_cast<org::apache::nifi::minifi::extensions::python::processors::ExecutePythonProcessor*>(DefautObjectFactory::createRaw(name, uuid));
    if (python_processor_type_ == PythonProcessorType::NIFI_TYPE) {
      auto name_tokens = org::apache::nifi::minifi::utils::StringUtils::split(name_, ".");
      ptr->setPythonClassName(name_tokens[name_tokens.size() - 1]);
      ptr->setPythonPaths(python_paths_);
    }
    ptr->initialize();
    ptr->setProperty(org::apache::nifi::minifi::extensions::python::processors::ExecutePythonProcessor::ScriptFile, file_);
    return dynamic_cast<org::apache::nifi::minifi::core::CoreComponent*>(ptr);
  }

 private:
  std::string file_;
  std::string name_;
  std::vector<std::filesystem::path> python_paths_;
  PythonProcessorType python_processor_type_;
};

#if defined(__GNUC__) || defined(__GNUG__)
#pragma GCC visibility pop
#endif
