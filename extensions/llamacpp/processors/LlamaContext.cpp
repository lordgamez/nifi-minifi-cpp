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

#include "LlamaContext.h"
#include "Exception.h"
#include "fmt/format.h"
#pragma push_macro("DEPRECATED")
#include "llama.h"
#pragma pop_macro("DEPRECATED")

namespace org::apache::nifi::minifi::extensions::llamacpp::processors {

static std::function<std::unique_ptr<LlamaContext>(const std::filesystem::path&, const LlamaSamplerParams&, const LlamaContextParams&)> test_provider;

void LlamaContext::testSetProvider(std::function<std::unique_ptr<LlamaContext>(const std::filesystem::path&, const LlamaSamplerParams&, const LlamaContextParams&)> provider) {
  test_provider = std::move(provider);
}

class DefaultLlamaContext : public LlamaContext {
 public:
  DefaultLlamaContext(const std::filesystem::path& model_path, const LlamaSamplerParams& llama_sampler_params, const LlamaContextParams& llama_ctx_params) {
    llama_backend_init();

    llama_model_ = llama_model_load_from_file(model_path.c_str(), llama_model_default_params());  // NOLINT(cppcoreguidelines-prefer-member-initializer)
    if (!llama_model_) {
      throw Exception(ExceptionType::PROCESS_SCHEDULE_EXCEPTION, fmt::format("Failed to load model from '{}'", model_path.c_str()));
    }

    llama_context_params ctx_params = llama_context_default_params();
    ctx_params.n_ctx = llama_ctx_params.n_ctx;
    ctx_params.n_batch = llama_ctx_params.n_batch;
    ctx_params.n_ubatch = llama_ctx_params.n_ubatch;
    ctx_params.n_seq_max = llama_ctx_params.n_seq_max;
    ctx_params.n_threads = llama_ctx_params.n_threads;
    ctx_params.n_threads_batch = llama_ctx_params.n_threads_batch;
    ctx_params.flash_attn = false;
    llama_ctx_ = llama_init_from_model(llama_model_, ctx_params);

    auto sparams = llama_sampler_chain_default_params();
    llama_sampler_ = llama_sampler_chain_init(sparams);

    if (llama_sampler_params.min_p) {
      llama_sampler_chain_add(llama_sampler_, llama_sampler_init_min_p(*llama_sampler_params.min_p, llama_sampler_params.min_keep));
    }
    if (llama_sampler_params.top_k) {
      llama_sampler_chain_add(llama_sampler_, llama_sampler_init_top_k(*llama_sampler_params.top_k));
    }
    if (llama_sampler_params.top_p) {
      llama_sampler_chain_add(llama_sampler_, llama_sampler_init_top_p(*llama_sampler_params.top_p, llama_sampler_params.min_keep));
    }
    if (llama_sampler_params.temperature) {
      llama_sampler_chain_add(llama_sampler_, llama_sampler_init_temp(*llama_sampler_params.temperature));
    }
    llama_sampler_chain_add(llama_sampler_, llama_sampler_init_dist(LLAMA_DEFAULT_SEED));
  }

  std::string applyTemplate(const std::vector<LlamaChatMessage>& messages) override {
    std::vector<llama_chat_message> llama_messages;
    llama_messages.reserve(messages.size());
    for (auto& msg : messages) {
      llama_messages.push_back(llama_chat_message{.role = msg.role.c_str(), .content = msg.content.c_str()});
    }
    std::string text;
    const char * chat_template = llama_model_chat_template(llama_model_, nullptr);
    int32_t res_size = llama_chat_apply_template(chat_template, llama_messages.data(), llama_messages.size(), true, text.data(), text.size());
    if (res_size > gsl::narrow<int32_t>(text.size())) {
      text.resize(res_size);
      llama_chat_apply_template(chat_template, llama_messages.data(), llama_messages.size(), true, text.data(), text.size());
    }
    text.resize(res_size);

    return text;
  }

  uint64_t generate(const std::string& input, std::function<void(std::string_view/*token*/)> token_handler) override {
    const llama_vocab * vocab = llama_model_get_vocab(llama_model_);
    std::vector<llama_token> enc_input = [&] {
      int32_t n_tokens = input.length() + 2;
      std::vector<llama_token> enc_input(n_tokens);
      n_tokens = llama_tokenize(vocab, input.data(), gsl::narrow<int32_t>(input.length()), enc_input.data(), gsl::narrow<int32_t>(enc_input.size()), true, true);
      if (n_tokens < 0) {
        enc_input.resize(-n_tokens);
        int32_t check = llama_tokenize(vocab, input.data(), gsl::narrow<int32_t>(input.length()), enc_input.data(), gsl::narrow<int32_t>(enc_input.size()), true, true);
        gsl_Assert(check == -n_tokens);
      } else {
        enc_input.resize(n_tokens);
      }
      return enc_input;
    }();

    llama_batch batch = llama_batch_get_one(enc_input.data(), gsl::narrow<int32_t>(enc_input.size()));
    llama_token new_token_id = 0;
    uint64_t tokens_generated = 0;
    while (true) {
      if (int32_t res = llama_decode(llama_ctx_, batch); res < 0) {
        throw std::logic_error("failed to execute decode");
      }

      new_token_id = llama_sampler_sample(llama_sampler_, llama_ctx_, -1);

      if (llama_vocab_is_eog(vocab, new_token_id)) {
        break;
      }

      ++tokens_generated;
      llama_sampler_accept(llama_sampler_, new_token_id);

      std::array<char, 128> buf{};
      int32_t len = llama_token_to_piece(vocab, new_token_id, buf.data(), buf.size(), 0, true);
      if (len < 0) {
        throw std::logic_error("failed to convert to text");
      }
      gsl_Assert(len < 128);

      std::string_view token_str{buf.data(), gsl::narrow<std::string_view::size_type>(len)};

      batch = llama_batch_get_one(&new_token_id, 1);

      token_handler(token_str);
    }

    return tokens_generated;
  }

  ~DefaultLlamaContext() override {
    llama_sampler_free(llama_sampler_);
    llama_sampler_ = nullptr;
    llama_free(llama_ctx_);
    llama_ctx_ = nullptr;
    llama_model_free(llama_model_);
    llama_model_ = nullptr;
    llama_backend_free();
  }

 private:
  llama_model* llama_model_{};
  llama_context* llama_ctx_{};
  llama_sampler* llama_sampler_{};
};

std::unique_ptr<LlamaContext> LlamaContext::create(const std::filesystem::path& model_path, const LlamaSamplerParams& llama_sampler_params, const LlamaContextParams& llama_ctx_params) {
  if (test_provider) {
    return test_provider(model_path, llama_sampler_params, llama_ctx_params);
  }
  return std::make_unique<DefaultLlamaContext>(model_path, llama_sampler_params, llama_ctx_params);
}

}  // namespace org::apache::nifi::minifi::extensions::llamacpp::processors
