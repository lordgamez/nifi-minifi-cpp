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

#include "LlamaCppProcessor.h"
#include "core/ProcessContext.h"
#include "core/ProcessSession.h"
#include "core/Resource.h"
#include "Exception.h"

#include "rapidjson/document.h"
#include "rapidjson/error/en.h"
#include "LlamaContext.h"

namespace org::apache::nifi::minifi::extensions::llamacpp::processors {

namespace {

std::optional<float> parseOptionalFloatProperty(const core::ProcessContext& context, const core::PropertyReference& property) {
  std::string str_value;
  if (!context.getProperty(property, str_value) || str_value.empty()) {
    return std::nullopt;
  }
  try {
    return std::stof(str_value);
  } catch(const std::exception&) {
    throw Exception(PROCESS_SCHEDULE_EXCEPTION, fmt::format("Property '{}' has invalid value '{}'", property.name, str_value));
  }
}

std::optional<int32_t> parseOptionalInt32Property(const core::ProcessContext& context, const core::PropertyReference& property) {
  std::string str_value;
  if (!context.getProperty(property, str_value) || str_value.empty()) {
    return std::nullopt;
  }
  try {
    return gsl::narrow<int32_t>(std::stoi(str_value));
  } catch(const std::exception&) {
    throw Exception(PROCESS_SCHEDULE_EXCEPTION, fmt::format("Property '{}' has invalid value '{}'", property.name, str_value));
  }
}

}  // namespace

void LlamaCppProcessor::initialize() {
  setSupportedProperties(Properties);
  setSupportedRelationships(Relationships);
}

void LlamaCppProcessor::onSchedule(core::ProcessContext& context, core::ProcessSessionFactory&) {
  model_path_.clear();
  context.getProperty(ModelPath, model_path_);
  context.getProperty(SystemPrompt, system_prompt_);

  LlamaSamplerParams llama_sampler_params;
  llama_sampler_params.temperature = parseOptionalFloatProperty(context, Temperature);
  llama_sampler_params.top_k = parseOptionalInt32Property(context, TopK);
  llama_sampler_params.top_p = parseOptionalFloatProperty(context, TopP);
  llama_sampler_params.min_p = parseOptionalFloatProperty(context, MinP);

  uint64_t uint_value = 0;
  if (context.getProperty(MinKeep, uint_value)) {
    llama_sampler_params.min_keep = uint_value;
  }

  LlamaContextParams llama_ctx_params;
  if (context.getProperty(TextContextSize, uint_value)) {
    llama_ctx_params.n_ctx = gsl::narrow_cast<uint32_t>(uint_value);
  }
  if (context.getProperty(LogicalMaximumBatchSize, uint_value)) {
    llama_ctx_params.n_batch = gsl::narrow_cast<uint32_t>(uint_value);
  }
  if (context.getProperty(PhysicalMaximumBatchSize, uint_value)) {
    llama_ctx_params.n_ubatch = gsl::narrow_cast<uint32_t>(uint_value);
  }
  if (context.getProperty(MaxNumberOfSequences, uint_value)) {
    llama_ctx_params.n_seq_max = gsl::narrow_cast<uint32_t>(uint_value);
  }
  int32_t int_value = 0;
  if (context.getProperty(ThreadsForGeneration, int_value)) {
    llama_ctx_params.n_threads = gsl::narrow_cast<int32_t>(int_value);
  }
  if (context.getProperty(ThreadsForBatchProcessing, int_value)) {
    llama_ctx_params.n_threads_batch = gsl::narrow_cast<int32_t>(int_value);
  }

  llama_ctx_ = LlamaContext::create(model_path_, llama_sampler_params, llama_ctx_params);
}

void LlamaCppProcessor::onTrigger(core::ProcessContext& context, core::ProcessSession& session) {
  auto input_ff = session.get();
  if (!input_ff) {
    context.yield();
    return;
  }
  auto ff_guard = gsl::finally([&] {
    session.remove(input_ff);
  });

  std::string prompt;
  context.getProperty(Prompt, prompt, input_ff.get());

  auto read_result = session.readBuffer(input_ff);
  std::string msg;
  if (read_result.buffer.size() > 0) {
    msg.append("Input data (or flowfile content):\n");
    msg.append({reinterpret_cast<const char*>(read_result.buffer.data()), read_result.buffer.size()});
    msg.append("\n\n");
  }
  msg.append(prompt);

  std::string input = [&] {
    std::vector<LlamaChatMessage> msgs;
    msgs.push_back({.role = "system", .content = system_prompt_.c_str()});
    msgs.push_back({.role = "user", .content = msg.c_str()});
    msgs.push_back({.role = "assisstant", .content = ""});

    return llama_ctx_->applyTemplate(msgs);
  }();

  logger_->log_debug("AI model input: {}", input);

  auto start_time = std::chrono::steady_clock::now();

  std::string text;
  llama_ctx_->generate(input, [&] (std::string_view token) {
    text += token;
  });

  auto elapsed_time = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start_time).count();
  logger_->log_debug("AI model inference time: {} ms", elapsed_time);
  logger_->log_debug("AI model output: {}", text);

  auto result = session.create();
  session.writeBuffer(result, text);
  session.transfer(result, Success);
}

void LlamaCppProcessor::notifyStop() {
  llama_ctx_.reset();
}

REGISTER_RESOURCE(LlamaCppProcessor, Processor);

}  // namespace org::apache::nifi::minifi::extensions::llamacpp::processors
