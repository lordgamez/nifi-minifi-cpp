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
  temperature_ = 0.8;
  context.getProperty(Temperature, temperature_);
  top_k_ = 40;
  context.getProperty(TopK, top_k_);
  top_p_ = 0.95;
  context.getProperty(TopP, top_p_);
  min_keep_ = 0;
  context.getProperty(MinKeep, min_keep_);
  seed_ = LLAMA_DEFAULT_SEED;
  context.getProperty(Seed, seed_);

  llama_ctx_ = llamacpp::LlamaContext::create(model_path_, gsl::narrow_cast<float>(temperature_), top_k_, gsl::narrow_cast<float>(top_p_), min_keep_, seed_);

  examples_.clear();
  std::string examples_str;
  context.getProperty(Examples, examples_str);
  if (examples_str.empty()) {
    return;
  }
  rapidjson::Document doc;
  rapidjson::ParseResult res = doc.Parse(examples_str.data(), examples_str.length());
  if (!res) {
    throw Exception(PROCESS_SCHEDULE_EXCEPTION, fmt::format("Malformed transformation example: {}", rapidjson::GetParseError_En(res.Code()), res.Offset()));
  }
  if (!doc.IsArray()) {
    throw Exception(PROCESS_SCHEDULE_EXCEPTION, "Malformed json example, expected array at /");
  }
  for (rapidjson::SizeType example_idx = 0; example_idx < doc.Size(); ++example_idx) {
    auto& example = doc[example_idx];
    if (!example.IsObject()) {
      throw Exception(PROCESS_SCHEDULE_EXCEPTION, fmt::format("Malformed json example, expected object at /{}", example_idx));
    }
    if (!example.HasMember("input") || !example["input"].IsObject()) {
      throw Exception(PROCESS_SCHEDULE_EXCEPTION, fmt::format("Malformed json example, expected object at /{}/input", example_idx));
    }
    if (!example.HasMember("output") || !example["output"].IsObject()) {
      throw Exception(PROCESS_SCHEDULE_EXCEPTION, fmt::format("Malformed json example, expected array at /{}/output", example_idx));
    }
    if (!example["input"].HasMember("role")) {
      throw Exception(PROCESS_SCHEDULE_EXCEPTION, fmt::format("Malformed json example, expected object at /{}/input/role", example_idx));
    }
    if (!example["input"].HasMember("content")) {
      throw Exception(PROCESS_SCHEDULE_EXCEPTION, fmt::format("Malformed json example, expected object at /{}/input/content", example_idx));
    }
    if (!example["output"].HasMember("role")) {
      throw Exception(PROCESS_SCHEDULE_EXCEPTION, fmt::format("Malformed json example, expected object at /{}/output/role", example_idx));
    }
    if (!example["output"].HasMember("content")) {
      throw Exception(PROCESS_SCHEDULE_EXCEPTION, fmt::format("Malformed json example, expected object at /{}/output/content", example_idx));
    }
    examples_.push_back(LLMExample{.input_role = example["input"]["role"].GetString(), .input = example["input"]["content"].GetString(),
                                   .output_role = example["output"]["role"].GetString(), .output = example["output"]["content"].GetString()});
  }
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
  std::string msg = "input data (or flowfile content): ";
  msg.append({reinterpret_cast<const char*>(read_result.buffer.data()), read_result.buffer.size()});

  std::string input = [&] {
    std::vector<llamacpp::LlamaChatMessage> msgs;
    msgs.push_back({.role = "system", .content = prompt.c_str()});
    for (auto& ex : examples_) {
      msgs.push_back({.role = "user", .content = ex.input.c_str()});
      msgs.push_back({.role = "assistant", .content = ex.output.c_str()});
    }
    msgs.push_back({.role = "system", .content = msg.c_str()});

    return llama_ctx_->applyTemplate(msgs);
  }();

  logger_->log_debug("AI model input: {}", input);

  std::string text;
  llama_ctx_->generate(input, [&] (std::string_view token) {
    text += token;
    return true;
  });

  logger_->log_debug("AI model output: {}", text);

  auto result = session.create();
  session.writeBuffer(result, text);
  session.transfer(result, Success);
}

void LlamaCppProcessor::notifyStop() {
  llama_ctx_.reset();
}

REGISTER_RESOURCE(LlamaCppProcessor, Processor);

}  // namespace org::apache::nifi::minifi::processors
