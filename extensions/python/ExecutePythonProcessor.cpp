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

#include <set>
#include <stdexcept>
#include <utility>

#include "ExecutePythonProcessor.h"
#include "types/PyRelationship.h"
#include "types/PyLogger.h"

#include "utils/StringUtils.h"
#include "utils/file/FileUtils.h"
#include "core/Resource.h"
#include "range/v3/range/conversion.hpp"
#include "range/v3/algorithm/find_if.hpp"

namespace org::apache::nifi::minifi::extensions::python::processors {

void ExecutePythonProcessor::initialize() {
  if (getProperties().empty()) {
    setSupportedProperties(Properties);
    setAcceptAllProperties();
    setSupportedRelationships(Relationships);
  }

  if (processor_initialized_) {
    logger_->log_debug("Processor has already been initialized, returning...");
    return;
  }

  try {
    loadScript();
  } catch(const std::runtime_error& err) {
    return;
  }

  // In case of native python processors we require initialization before onSchedule
  // so that we can provide manifest of processor identity on C2
  python_script_engine_ = createScriptEngine();
  initalizeThroughScriptEngine();
}

void ExecutePythonProcessor::initalizeThroughScriptEngine() {
  appendPathForImportModules();
  python_script_engine_->appendModulePaths(python_paths_);
  if (!script_file_path_.empty() && !PythonScriptEngine::virtualenv_path_.empty() && PythonScriptEngine::install_python_packages_automatically_) {
    auto requirements_file_path = std::filesystem::path(script_file_path_).parent_path() / "requirements.txt";
    if (std::filesystem::exists(requirements_file_path)) {
      installPythonRequirementsFromFile(requirements_file_path);
    }
  }
  python_script_engine_->eval(script_to_exec_);
  if (python_class_name_) {
    python_script_engine_->initializeProcessorObject(*python_class_name_);
  }
  python_script_engine_->describe(this);
  python_script_engine_->onInitialize(this);
  processor_initialized_ = true;
}

void ExecutePythonProcessor::onScheduleSharedPtr(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::ProcessSessionFactory>& /*sessionFactory*/) {
  setAutoTerminatedRelationships(std::vector<core::Relationship>{Original});
  if (!processor_initialized_) {
    loadScript();
    python_script_engine_ = createScriptEngine();
    initalizeThroughScriptEngine();
  } else {
    reloadScriptIfUsingScriptFileProperty();
    if (script_to_exec_.empty()) {
      throw std::runtime_error("Neither Script Body nor Script File is available to execute");
    }
  }

  gsl_Expects(python_script_engine_);
  python_script_engine_->eval(script_to_exec_);
  python_script_engine_->onSchedule(context);

  getProperty(ReloadOnScriptChange, reload_on_script_change_);
}

void ExecutePythonProcessor::onTriggerSharedPtr(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::ProcessSession> &session) {
  reloadScriptIfUsingScriptFileProperty();
  if (script_to_exec_.empty()) {
    throw std::runtime_error("Neither Script Body nor Script File is available to execute");
  }

  python_script_engine_->onTrigger(context, session);
}

void ExecutePythonProcessor::appendPathForImportModules() {
  std::string module_directory;
  getProperty(ModuleDirectory, module_directory);
  if (!module_directory.empty()) {
    python_script_engine_->appendModulePaths(utils::string::splitAndTrimRemovingEmpty(module_directory, ",") | ranges::to<std::vector<std::filesystem::path>>());
  }
}

void ExecutePythonProcessor::loadScriptFromFile() {
  std::ifstream file_handle(script_file_path_);
  if (!file_handle.is_open()) {
    script_to_exec_ = "";
    throw std::runtime_error("Failed to read Script File: " + script_file_path_);
  }
  script_to_exec_ = std::string{ (std::istreambuf_iterator<char>(file_handle)), (std::istreambuf_iterator<char>()) };
}

void ExecutePythonProcessor::loadScript() {
  std::string script_file;
  std::string script_body;
  getProperty(ScriptFile, script_file);
  getProperty(ScriptBody, script_body);
  if (script_file.empty() && script_body.empty()) {
    throw std::runtime_error("Neither Script Body nor Script File is available to execute");
  }

  if (!script_file.empty()) {
    if (!script_body.empty()) {
      throw std::runtime_error("Only one of Script File or Script Body may be used");
    }
    script_file_path_ = script_file;
    loadScriptFromFile();
    last_script_write_time_ = utils::file::last_write_time(script_file_path_);
    return;
  }
  script_to_exec_ = script_body;
}

void ExecutePythonProcessor::reloadScriptIfUsingScriptFileProperty() {
  if (script_file_path_.empty() || !reload_on_script_change_) {
    return;
  }
  auto file_write_time = utils::file::last_write_time(script_file_path_);
  if (file_write_time != last_script_write_time_) {
    logger_->log_debug("Script file has changed since last time, reloading...");
    loadScriptFromFile();
    last_script_write_time_ = file_write_time;
    python_script_engine_->eval(script_to_exec_);
  }
}

std::unique_ptr<PythonScriptEngine> ExecutePythonProcessor::createScriptEngine() {
  auto engine = std::make_unique<PythonScriptEngine>();

  python_logger_ = core::logging::LoggerFactory<ExecutePythonProcessor>::getAliasedLogger(getName());
  engine->initialize(Success, Failure, Original, python_logger_);

  return engine;
}

core::Property* ExecutePythonProcessor::findProperty(const std::string& name) const {
  if (auto prop_ptr = core::ConfigurableComponent::findProperty(name)) {
    return prop_ptr;
  }

  auto it = ranges::find_if(python_properties_, [&name](const auto& item){
    return item.getName() == name;
  });
  if (it != python_properties_.end()) {
    return const_cast<core::Property*>(&*it);
  }

  return nullptr;
}

std::map<std::string, core::Property> ExecutePythonProcessor::getProperties() const {
  auto result = ConfigurableComponent::getProperties();

  std::lock_guard<std::mutex> lock(configuration_mutex_);

  for (const auto &property : python_properties_) {
    result.insert({ property.getName(), property });
  }

  return result;
}

void ExecutePythonProcessor::installPythonRequirementsFromFile(const std::filesystem::path& requirements_file_path) {
  if (PythonScriptEngine::virtualenv_path_.empty() || !PythonScriptEngine::install_python_packages_automatically_) {
    return;
  }
  std::string pip_command;
#if WIN32
    pip_command.append((PythonScriptEngine::virtualenv_path_ / "Scripts" / "activate.bat").string()).append(" && ");
#else
    pip_command.append(". ").append((PythonScriptEngine::virtualenv_path_ / "bin" / "activate").string()).append(" && ");
#endif
  pip_command.append(PythonScriptEngine::python_binary_).append(" -m pip install --no-cache-dir -r \"").append(requirements_file_path.string()).append("\"");
  auto return_value = std::system(pip_command.c_str());
  if (return_value != 0) {
    throw PythonScriptException(fmt::format("The following command to install python packages failed: '{}'", pip_command));
  }
}

REGISTER_RESOURCE(ExecutePythonProcessor, Processor);

}  // namespace org::apache::nifi::minifi::extensions::python::processors
