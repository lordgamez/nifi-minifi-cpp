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
#include "unit/TestBase.h"
#include "unit/Catch.h"
#include "LlamaCppProcessor.h"
#include "unit/SingleProcessorTestController.h"
#include "core/FlowFile.h"

namespace org::apache::nifi::minifi::extensions::llamacpp::test {

class MockLlamaContext : public processors::LlamaContext {
 public:
  std::string applyTemplate(const std::vector<processors::LlamaChatMessage>& messages) override {
    messages_ = messages;
    return "Test input";
  }

  uint64_t generate(const std::string& input, std::function<void(std::string_view/*token*/)> token_handler) override {
    input_ = input;
    token_handler("Test ");
    token_handler("generated");
    token_handler(" content");
    return 3;
  }

  const std::vector<processors::LlamaChatMessage>& getMessages() const {
    return messages_;
  }

  const std::string& getInput() const {
    return input_;
  }

  ~MockLlamaContext() override = default;

 private:
  std::vector<processors::LlamaChatMessage> messages_;
  std::string input_;
};

TEST_CASE("Prompt is generated correctly with default parameters") {
  auto mock_llama_context = std::make_unique<MockLlamaContext>();
  auto mock_llama_context_ptr = mock_llama_context.get();
  std::filesystem::path test_model_path;
  processors::LlamaSamplerParams test_sampler_params;
  processors::LlamaContextParams test_context_params;
  int32_t test_n_gpu_layers = 0;
  processors::LlamaContext::testSetProvider(
    [&](const std::filesystem::path& model_path, const processors::LlamaSamplerParams& sampler_params, const processors::LlamaContextParams& context_params, int32_t gpu_layers) {
      test_model_path = model_path;
      test_sampler_params = sampler_params;
      test_context_params = context_params;
      test_n_gpu_layers = gpu_layers;
      return std::move(mock_llama_context);
    }
  );
  minifi::test::SingleProcessorTestController controller(std::make_unique<processors::LlamaCppProcessor>("LlamaCppProcessor"));
  LogTestController::getInstance().setTrace<processors::LlamaCppProcessor>();
  controller.getProcessor()->setProperty(processors::LlamaCppProcessor::ModelPath, "Dummy model");
  controller.getProcessor()->setProperty(processors::LlamaCppProcessor::Prompt, "Question: What is the answer to life, the universe and everything?");

  auto results = controller.trigger(minifi::test::InputFlowFileData{.content = "42", .attributes = {}});
  CHECK(test_model_path == "Dummy model");
  CHECK(test_sampler_params.temperature == 0.8f);
  CHECK(test_sampler_params.top_k == 40);
  CHECK(test_sampler_params.top_p == 0.9f);
  CHECK(test_sampler_params.min_p == std::nullopt);
  CHECK(test_sampler_params.min_keep == 0);
  CHECK(test_context_params.n_ctx == 512);
  CHECK(test_context_params.n_batch == 2048);
  CHECK(test_context_params.n_ubatch == 512);
  CHECK(test_context_params.n_seq_max == 1);
  CHECK(test_context_params.n_threads == 4);
  CHECK(test_context_params.n_threads_batch == 4);
  CHECK(test_n_gpu_layers == -1);

  REQUIRE(results.at(processors::LlamaCppProcessor::Success).size() == 1);
  auto& output_flow_file = results.at(processors::LlamaCppProcessor::Success)[0];
  CHECK(controller.plan->getContent(output_flow_file) == "Test generated content");
  CHECK(mock_llama_context_ptr->getInput() == "Test input");
  CHECK(mock_llama_context_ptr->getMessages().size() == 3);
  CHECK(mock_llama_context_ptr->getMessages()[0].role == "system");
  CHECK(mock_llama_context_ptr->getMessages()[0].content == "You are a helpful assisstant. You are given a question with some possible input data otherwise called flowfile data. "
                                                            "You are expected to generate a response based on the quiestion and the input data.");
  CHECK(mock_llama_context_ptr->getMessages()[1].role == "user");
  CHECK(mock_llama_context_ptr->getMessages()[1].content == "Input data (or flowfile content): 42\n\nQuestion: What is the answer to life, the universe and everything?");
  CHECK(mock_llama_context_ptr->getMessages()[2].role == "assisstant");
  CHECK(mock_llama_context_ptr->getMessages()[2].content.empty());
}

TEST_CASE("Prompt is generated correctly with custom parameters") {
  auto mock_llama_context = std::make_unique<MockLlamaContext>();
  auto mock_llama_context_ptr = mock_llama_context.get();
  std::filesystem::path test_model_path;
  processors::LlamaSamplerParams test_sampler_params;
  processors::LlamaContextParams test_context_params;
  int32_t test_n_gpu_layers = 0;
  processors::LlamaContext::testSetProvider(
    [&](const std::filesystem::path& model_path, const processors::LlamaSamplerParams& sampler_params, const processors::LlamaContextParams& context_params, int32_t gpu_layers) {
      test_model_path = model_path;
      test_sampler_params = sampler_params;
      test_context_params = context_params;
      test_n_gpu_layers = gpu_layers;
      return std::move(mock_llama_context);
    }
  );
  minifi::test::SingleProcessorTestController controller(std::make_unique<processors::LlamaCppProcessor>("LlamaCppProcessor"));
  LogTestController::getInstance().setTrace<processors::LlamaCppProcessor>();
  controller.getProcessor()->setProperty(processors::LlamaCppProcessor::ModelPath, "/path/to/model");
  controller.getProcessor()->setProperty(processors::LlamaCppProcessor::Prompt, "Question: What is the answer to life, the universe and everything?");
  controller.getProcessor()->setProperty(processors::LlamaCppProcessor::Temperature, "0.4");
  controller.getProcessor()->setProperty(processors::LlamaCppProcessor::TopK, "20");
  controller.getProcessor()->setProperty(processors::LlamaCppProcessor::TopP, "");
  controller.getProcessor()->setProperty(processors::LlamaCppProcessor::MinP, "0.1");
  controller.getProcessor()->setProperty(processors::LlamaCppProcessor::MinKeep, "1");
  controller.getProcessor()->setProperty(processors::LlamaCppProcessor::TextContextSize, "4096");
  controller.getProcessor()->setProperty(processors::LlamaCppProcessor::LogicalMaximumBatchSize, "1024");
  controller.getProcessor()->setProperty(processors::LlamaCppProcessor::PhysicalMaximumBatchSize, "796");
  controller.getProcessor()->setProperty(processors::LlamaCppProcessor::MaxNumberOfSequences, "2");
  controller.getProcessor()->setProperty(processors::LlamaCppProcessor::ThreadsForGeneration, "12");
  controller.getProcessor()->setProperty(processors::LlamaCppProcessor::ThreadsForBatchProcessing, "8");
  controller.getProcessor()->setProperty(processors::LlamaCppProcessor::NumberOfGPULayers, "10");
  controller.getProcessor()->setProperty(processors::LlamaCppProcessor::SystemPrompt, "Whatever");

  auto results = controller.trigger(minifi::test::InputFlowFileData{.content = "42", .attributes = {}});
  CHECK(test_model_path == "/path/to/model");
  CHECK(test_sampler_params.temperature == 0.4f);
  CHECK(test_sampler_params.top_k == 20);
  CHECK(test_sampler_params.top_p == std::nullopt);
  CHECK(test_sampler_params.min_p == 0.1f);
  CHECK(test_sampler_params.min_keep == 1);
  CHECK(test_context_params.n_ctx == 4096);
  CHECK(test_context_params.n_batch == 1024);
  CHECK(test_context_params.n_ubatch == 796);
  CHECK(test_context_params.n_seq_max == 2);
  CHECK(test_context_params.n_threads == 12);
  CHECK(test_context_params.n_threads_batch == 8);
  CHECK(test_n_gpu_layers == 10);

  REQUIRE(results.at(processors::LlamaCppProcessor::Success).size() == 1);
  auto& output_flow_file = results.at(processors::LlamaCppProcessor::Success)[0];
  CHECK(controller.plan->getContent(output_flow_file) == "Test generated content");
  CHECK(mock_llama_context_ptr->getInput() == "Test input");
  CHECK(mock_llama_context_ptr->getMessages().size() == 3);
  CHECK(mock_llama_context_ptr->getMessages()[0].role == "system");
  CHECK(mock_llama_context_ptr->getMessages()[0].content == "Whatever");
  CHECK(mock_llama_context_ptr->getMessages()[1].role == "user");
  CHECK(mock_llama_context_ptr->getMessages()[1].content == "Input data (or flowfile content): 42\n\nQuestion: What is the answer to life, the universe and everything?");
  CHECK(mock_llama_context_ptr->getMessages()[2].role == "assisstant");
  CHECK(mock_llama_context_ptr->getMessages()[2].content.empty());
}

TEST_CASE("Invalid values for optional double type properties throw exception") {
  processors::LlamaContext::testSetProvider(
    [&](const std::filesystem::path&, const processors::LlamaSamplerParams&, const processors::LlamaContextParams&, int32_t) {
      return std::make_unique<MockLlamaContext>();
    }
  );
  minifi::test::SingleProcessorTestController controller(std::make_unique<processors::LlamaCppProcessor>("LlamaCppProcessor"));
  LogTestController::getInstance().setTrace<processors::LlamaCppProcessor>();
  controller.getProcessor()->setProperty(processors::LlamaCppProcessor::ModelPath, "Dummy model");
  controller.getProcessor()->setProperty(processors::LlamaCppProcessor::Prompt, "Question: What is the answer to life, the universe and everything?");

  std::string property_name;
  SECTION("Invalid value for Temperature property") {
    controller.getProcessor()->setProperty(processors::LlamaCppProcessor::Temperature, "invalid_value");
    property_name = processors::LlamaCppProcessor::Temperature.name;
  }
  SECTION("Invalid value for Top P property") {
    controller.getProcessor()->setProperty(processors::LlamaCppProcessor::TopP, "invalid_value");
    property_name = processors::LlamaCppProcessor::TopP.name;
  }
  SECTION("Invalid value for Min P property") {
    controller.getProcessor()->setProperty(processors::LlamaCppProcessor::MinP, "invalid_value");
    property_name = processors::LlamaCppProcessor::MinP.name;
  }

  REQUIRE_THROWS_WITH(controller.trigger(minifi::test::InputFlowFileData{.content = "42", .attributes = {}}), fmt::format("Process Schedule Operation: Property '{}' has invalid value 'invalid_value'", property_name));
}

TEST_CASE("Top K property empty and invalid values are handled properly") {
  processors::LlamaSamplerParams sampler_params;
  std::optional<int32_t> test_top_k;
  processors::LlamaContext::testSetProvider(
    [&](const std::filesystem::path&, const processors::LlamaSamplerParams& sampler_params, const processors::LlamaContextParams&, int32_t) {
      test_top_k = sampler_params.top_k;
      return std::make_unique<MockLlamaContext>();
    }
  );
  minifi::test::SingleProcessorTestController controller(std::make_unique<processors::LlamaCppProcessor>("LlamaCppProcessor"));
  LogTestController::getInstance().setTrace<processors::LlamaCppProcessor>();
  controller.getProcessor()->setProperty(processors::LlamaCppProcessor::ModelPath, "Dummy model");
  controller.getProcessor()->setProperty(processors::LlamaCppProcessor::Prompt, "Question: What is the answer to life, the universe and everything?");
  SECTION("Empty value for Top K property") {
    controller.getProcessor()->setProperty(processors::LlamaCppProcessor::TopK, "");
    auto results = controller.trigger(minifi::test::InputFlowFileData{.content = "42", .attributes = {}});
    REQUIRE(test_top_k == std::nullopt);
  }
  SECTION("Invalid value for Top K property") {
    controller.getProcessor()->setProperty(processors::LlamaCppProcessor::TopK, "invalid_value");
    REQUIRE_THROWS_WITH(controller.trigger(minifi::test::InputFlowFileData{.content = "42", .attributes = {}}), "Process Schedule Operation: Property 'Top K' has invalid value 'invalid_value'");
  }
}

}  // namespace org::apache::nifi::minifi::extensions::llamacpp::test
