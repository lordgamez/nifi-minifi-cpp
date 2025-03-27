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

#include "core/Processor.h"
#include "core/logging/LoggerFactory.h"
#include "core/PropertyDefinitionBuilder.h"
#include "LlamaContext.h"
#pragma push_macro("DEPRECATED")
#include "llama.h"
#pragma pop_macro("DEPRECATED")

namespace org::apache::nifi::minifi::processors {

class LlamaCppProcessor : public core::ProcessorImpl {
  struct LLMExample {
    std::string input_role;
    std::string input;
    std::string output_role;
    std::string output;
  };

 public:
  explicit LlamaCppProcessor(std::string_view name, const utils::Identifier& uuid = {})
      : core::ProcessorImpl(name, uuid) {
  }
  ~LlamaCppProcessor() override = default;

  EXTENSIONAPI static constexpr const char* Description = "LlamaCpp processor";

  EXTENSIONAPI static constexpr auto ModelPath = core::PropertyDefinitionBuilder<>::createProperty("Model Path")
      .withDescription("The filesystem path of the model")
      .isRequired(true)
      .build();
  EXTENSIONAPI static constexpr auto Temperature = core::PropertyDefinitionBuilder<>::createProperty("Temperature")
      .withDescription("The inference temperature")
      .isRequired(true)
      .withDefaultValue("0.8")
      .build();
  EXTENSIONAPI static constexpr auto TopK = core::PropertyDefinitionBuilder<>::createProperty("Top K")
      .withDescription("Limit the next token selection to the K most probable tokens.")
      .isRequired(true)
      .withPropertyType(core::StandardPropertyTypes::UNSIGNED_INT_TYPE)
      .withDefaultValue("40")
      .build();
  EXTENSIONAPI static constexpr auto TopP = core::PropertyDefinitionBuilder<>::createProperty("Top P")
      .withDescription("Limit the next token selection to a subset of tokens with a cumulative probability above a threshold P")
      .isRequired(true)
      .withDefaultValue("0.95")
      .build();
  EXTENSIONAPI static constexpr auto MinKeep = core::PropertyDefinitionBuilder<>::createProperty("Min Keep")
      .withDescription("If greater than 0, force samplers to return N possible tokens at minimum.")
      .isRequired(true)
      .withDefaultValue("0")
      .build();
  EXTENSIONAPI static constexpr auto Seed = core::PropertyDefinitionBuilder<>::createProperty("Seed")
      .withDescription("Set RNG seed, if not set the default LLAMA seed will be used")
      .withPropertyType(core::StandardPropertyTypes::UNSIGNED_LONG_TYPE)
      .build();
  EXTENSIONAPI static constexpr auto Prompt = core::PropertyDefinitionBuilder<>::createProperty("Prompt")
      .withDescription("The prompt for the inference")
      .supportsExpressionLanguage(true)
      .isRequired(true)
      .build();
  EXTENSIONAPI static constexpr auto SystemPrompt = core::PropertyDefinitionBuilder<>::createProperty("System Prompt")
      .withDescription("The system prompt for the inference")
      .withDefaultValue("You are a helpful assisstant. You are given a question with some possible input data otherwise called flowfile data. "
                        "You are expected to generate a response based on the quiestion and the input data.")
      .isRequired(true)
      .build();
  EXTENSIONAPI static constexpr auto Properties = std::to_array<core::PropertyReference>({
    ModelPath,
    Temperature,
    TopK,
    TopP,
    MinKeep,
    Seed,
    Prompt,
    SystemPrompt
  });


  EXTENSIONAPI static constexpr auto Success = core::RelationshipDefinition{"success", "Generated result from the model"};
  EXTENSIONAPI static constexpr auto Relationships = std::array{Success};

  EXTENSIONAPI static constexpr bool SupportsDynamicProperties = false;
  EXTENSIONAPI static constexpr bool SupportsDynamicRelationships = true;
  EXTENSIONAPI static constexpr core::annotation::Input InputRequirement = core::annotation::Input::INPUT_REQUIRED;
  EXTENSIONAPI static constexpr bool IsSingleThreaded = true;

  ADD_COMMON_VIRTUAL_FUNCTIONS_FOR_PROCESSORS

  void onSchedule(core::ProcessContext& context, core::ProcessSessionFactory& session_factory) override;
  void onTrigger(core::ProcessContext& context, core::ProcessSession& session) override;
  void initialize() override;
  void notifyStop() override;

 private:
  std::shared_ptr<core::logging::Logger> logger_ = core::logging::LoggerFactory<LlamaCppProcessor>::getLogger(uuid_);

  double temperature_{0.8};
  uint64_t top_k_{40};
  double top_p_{0.95};
  uint64_t min_keep_{0};
  uint64_t seed_{LLAMA_DEFAULT_SEED};
  std::string model_path_;
  std::vector<LLMExample> examples_;
  std::string system_prompt_;

  std::unique_ptr<llamacpp::LlamaContext> llama_ctx_;
};

}  // namespace org::apache::nifi::minifi::processors
