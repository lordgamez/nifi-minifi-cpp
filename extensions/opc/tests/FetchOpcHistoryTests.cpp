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
#include "OpcUaTestServer.h"
#include "unit/SingleProcessorTestController.h"
#include "include/FetchOpcHistory.h"
#include "unit/TestUtils.h"

namespace org::apache::nifi::minifi::test {

TEST_CASE("Test fetching full history of a freshly created node", "[fetchopchistory]") {
  OpcUaTestServer server(4841);
  server.start();
  SingleProcessorTestController controller{minifi::test::utils::make_processor<processors::FetchOpcHistory>("FetchOpcHistory")};
  auto fetch_opc_processor = controller.getProcessor();
  REQUIRE(fetch_opc_processor->setProperty(processors::FetchOpcHistory::OPCServerEndPoint.name, "opc.tcp://127.0.0.1:4841/"));
  REQUIRE(fetch_opc_processor->setProperty(processors::FetchOpcHistory::NodeIDType.name, "String"));
  REQUIRE(fetch_opc_processor->setProperty(processors::FetchOpcHistory::NodeID.name, "INT1"));
  REQUIRE(fetch_opc_processor->setProperty(processors::FetchOpcHistory::NameSpaceIndex.name, std::to_string(server.getNamespaceIndex())));

  const auto results = controller.trigger();
  REQUIRE(results.at(processors::FetchOpcHistory::Success).size() == 1);
  auto flow_file = results.at(processors::FetchOpcHistory::Success)[0];
  CHECK(controller.plan->getContent(flow_file) == "1");
  CHECK(flow_file->getAttribute("ModificationUsername") == "test_user");
  CHECK(flow_file->getAttribute("ModificationUpdateType") == "Replace");
  CHECK(flow_file->getAttribute("ModificationTime") == "2024-06-15T10:30:00.000Z");
}

}  // namespace org::apache::nifi::minifi::test
