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

#include <memory>
#include <filesystem>
#include <vector>
#include <string_view>
#include <string>
#include <functional>

namespace org::apache::nifi::minifi::extensions::llamacpp::processors {

struct LlamaChatMessage {
  std::string role;
  std::string content;
};

struct LlamaSamplerParams {
  float temperature = 0.8f;
  int32_t top_k = 40;
  float top_p = 0.9f;
  float min_p = 0.0f;
  uint64_t min_keep = 0;
};

struct LlamaContextParams {
  uint32_t n_ctx = 512;
  uint32_t n_batch = 2048;
  uint32_t n_ubatch = 512;
  uint32_t n_seq_max = 1;
  int32_t n_threads = 4;
  int32_t n_threads_batch = 4;
};

class LlamaContext {
 public:
  static void testSetProvider(std::function<std::unique_ptr<LlamaContext>(const std::filesystem::path&, const LlamaSamplerParams&, const LlamaContextParams&, int32_t)> provider);
  static std::unique_ptr<LlamaContext> create(const std::filesystem::path& model_path, const LlamaSamplerParams& llama_sampler_params, const LlamaContextParams& llama_ctx_params, int32_t n_gpu_layers);
  virtual std::string applyTemplate(const std::vector<LlamaChatMessage>& messages) = 0;
  virtual uint64_t generate(const std::string& input, std::function<void(std::string_view/*token*/)> token_handler) = 0;
  virtual ~LlamaContext() = default;
};

}  // namespace org::apache::nifi::minifi::extensions::llamacpp::processors
