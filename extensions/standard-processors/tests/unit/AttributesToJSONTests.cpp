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
#include "TestBase.h"
#include "utils/TestUtils.h"
#include "AttributesToJSON.h"
#include "GenerateFlowFile.h"
#include "PutFile.h"
#include "UpdateAttribute.h"

class AttributesToJSONTestFixture {
 public:
  AttributesToJSONTestFixture() {
    LogTestController::getInstance().setTrace<TestPlan>();
    LogTestController::getInstance().setDebug<minifi::processors::AttributesToJSON>();
    LogTestController::getInstance().setDebug<minifi::processors::GenerateFlowFile>();
    LogTestController::getInstance().setDebug<minifi::processors::PutFile>();
    LogTestController::getInstance().setDebug<minifi::processors::UpdateAttribute>();

    auto dir = minifi::utils::createTempDir(&test_controller_);

    plan_ = test_controller_.createPlan();
    auto genfile = plan_->addProcessor("GenerateFlowFile", "GenerateFlowFile");
    auto update_attribute = plan_->addProcessor("UpdateAttribute", "UpdateAttribute", core::Relationship("success", "description"), true);
    auto attribute_to_json = plan_->addProcessor("AttributesToJSON", "AttributesToJSON", core::Relationship("success", "description"), true);
    auto putfile = plan_->addProcessor("PutFile", "PutFile", core::Relationship("success", "description"), true);

    plan_->setProperty(genfile, org::apache::nifi::minifi::processors::GenerateFlowFile::FileSize.getName(), "0B");
    plan_->setProperty(putfile, org::apache::nifi::minifi::processors::PutFile::Directory.getName(), dir);

    update_attribute->setDynamicProperty("my_attribute", "my_value");
    update_attribute->setDynamicProperty("other_attribute", "other_value");

    std::vector<std::string> file_contents;

    auto lambda = [&file_contents](const std::string& path, const std::string& filename) -> bool {
      std::ifstream is(path + utils::file::FileUtils::get_separator() + filename, std::ifstream::binary);
      std::string file_content((std::istreambuf_iterator<char>(is)), std::istreambuf_iterator<char>());
      file_contents.push_back(file_content);
      return true;
    };

    utils::file::FileUtils::list_dir(dir, lambda, plan_->getLogger(), false);

    REQUIRE(file_contents.size() == 1);
    REQUIRE(file_contents[0].size() == 10);
  }

  void run() {
    test_controller_.runSession(plan_);
  }

 private:
  TestController test_controller_;
  std::shared_ptr<TestPlan> plan_;
};

TEST_CASE_METHOD(AttributesToJSONTestFixture, "Move all attributes to a flowfile attribute", "[AttributesToJSONTests]") {
  run();
}
