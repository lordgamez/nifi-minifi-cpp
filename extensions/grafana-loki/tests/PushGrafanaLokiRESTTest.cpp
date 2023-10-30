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

#include "../PushGrafanaLokiREST.h"
#include "MockGrafanaLoki.h"
#include "SingleProcessorTestController.h"
#include "Catch.h"

namespace org::apache::nifi::minifi::extensions::grafana::loki::test {

TEST_CASE("PushGrafanaLokiREST", "[loki]") {
  MockGrafanaLoki mock_loki("3100");

  auto push_grafana_loki_rest = std::make_shared<PushGrafanaLokiREST>("PushGrafanaLokiREST");
  minifi::test::SingleProcessorTestController test_controller{push_grafana_loki_rest};
  CHECK(test_controller.plan->setProperty(push_grafana_loki_rest, PushGrafanaLokiREST::Url, "localhost:3100"));
  CHECK(test_controller.plan->setProperty(push_grafana_loki_rest, PushGrafanaLokiREST::StreamLabels, "job=minifi,directory=/opt/minifi/logs/"));

  SECTION("test") {
    // CHECK(test_controller.plan->setProperty(elasticsearch_credentials_controller_service,
    //                                         ElasticsearchCredentialsControllerService::Username,
    //                                         MockElasticAuthHandler::USERNAME));
    // CHECK(test_controller.plan->setProperty(elasticsearch_credentials_controller_service,
    //                                         ElasticsearchCredentialsControllerService::Password,
    //                                         MockElasticAuthHandler::PASSWORD));

    // auto results = test_controller.trigger({{R"({"field1":"value1"}")", {{"elastic_action", "index"}}},
    //                                         {R"({"field1":"value2"}")", {{"elastic_action", "index"}}}});
    // REQUIRE(results[PushGrafanaLokiREST::Success].size() == 2);
    // for (const auto& result : results[PushGrafanaLokiREST::Success]) {
    //   auto attributes = result->getAttributes();
    //   CHECK(attributes.contains("elasticsearch.index._id"));
    //   CHECK(attributes.contains("elasticsearch.index._index"));
    // }
  }
}

}  // namespace org::apache::nifi::minifi::extensions::grafana::loki::test
