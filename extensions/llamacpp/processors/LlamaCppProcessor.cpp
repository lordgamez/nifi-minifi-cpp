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

namespace org::apache::nifi::minifi::processors {

void LlamaCppProcessor::initialize() {
  setSupportedProperties(Properties);
  setSupportedRelationships(Relationships);
}

void LlamaCppProcessor::onSchedule(core::ProcessContext& context, core::ProcessSessionFactory&) {
  model_path_.clear();
  context.getProperty(ModelPath, model_path_);
  context.getProperty(SystemPrompt, system_prompt_);

  llamacpp::LlamaSamplerParams llama_sampler_params;
  double double_value = 0.0f;
  if (context.getProperty(Temperature, double_value)) {
    llama_sampler_params.temperature = gsl::narrow_cast<float>(double_value);
  }

  int64_t int_value = 0;
  if (context.getProperty(TopK, int_value)) {
    llama_sampler_params.top_k = gsl::narrow_cast<int32_t>(int_value);
  }

  if (context.getProperty(TopP, double_value)) {
    llama_sampler_params.top_p = gsl::narrow_cast<float>(double_value);
  }

  if (context.getProperty(MinP, double_value)) {
    llama_sampler_params.min_p = gsl::narrow_cast<float>(double_value);
  }

  uint64_t uint_value = 0;
  if (context.getProperty(MinKeep, uint_value)) {
    llama_sampler_params.min_keep = uint_value;
  }

  llamacpp::LlamaContextParams llama_ctx_params;
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
  if (context.getProperty(ThreadsForGeneration, int_value)) {
    llama_ctx_params.n_threads = gsl::narrow_cast<int32_t>(int_value);
  }
  if (context.getProperty(ThreadsForBatchProcessing, int_value)) {
    llama_ctx_params.n_threads_batch = gsl::narrow_cast<int32_t>(int_value);
  }

  int32_t n_gpu_layers = -1;
  if (context.getProperty(NumberOfGPULayers, int_value)) {
    n_gpu_layers = gsl::narrow_cast<int32_t>(int_value);
  }

  llama_ctx_ = llamacpp::LlamaContext::create(model_path_, llama_sampler_params, llama_ctx_params, n_gpu_layers);
}

void LlamaCppProcessor::onTrigger(core::ProcessContext& context, core::ProcessSession& session) {
  auto start_time = std::chrono::steady_clock::now();
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
  std::string msg = "input data (or flowfile content): ";
  msg.append({reinterpret_cast<const char*>(read_result.buffer.data()), read_result.buffer.size()});
  msg = prompt + "\n\n" + msg;

  std::string input = [&] {
    std::vector<llamacpp::LlamaChatMessage> msgs;
    msgs.push_back({.role = "system", .content = system_prompt_.c_str()});
    msgs.push_back({.role = "user", .content = msg.c_str()});
    msgs.push_back({.role = "assisstant", .content = ""});

    return llama_ctx_->applyTemplate(msgs);
  }();

  logger_->log_info("AI model input: {}", input);

  std::string text;
  llama_ctx_->generate(input, [&] (std::string_view token) {
    text += token;
    return true;
  });

  logger_->log_info("AI model output: {}", text);

  auto result = session.create();
  session.writeBuffer(result, text);
  session.transfer(result, Success);
  auto end_time = std::chrono::steady_clock::now();
  auto elapsed_time = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count();
  logger_->log_info("AI model inference time: {} ms", elapsed_time);
}

void LlamaCppProcessor::notifyStop() {
  llama_ctx_.reset();
}

REGISTER_RESOURCE(LlamaCppProcessor, Processor);

}  // namespace org::apache::nifi::minifi::processors
