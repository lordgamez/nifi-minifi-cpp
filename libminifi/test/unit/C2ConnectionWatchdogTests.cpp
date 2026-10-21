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
#include "c2/C2ConnectionWatchdog.h"
#include "unit/Catch.h"
#include "unit/TestBase.h"

using namespace std::literals::chrono_literals;

namespace org::apache::nifi::minifi::test {

constexpr ShutdownType EXPECTED_SHUTDOWN_TYPE = ShutdownType::SHUTDOWN;
constexpr int32_t EXPECTED_EXIT_CODE = 90;

TEST_CASE("Test invalid values", "[c2watchdog]") {
  LogTestController::getInstance().reset();
  SECTION("Zero timeout value") {
    REQUIRE_THROWS_WITH(c2::C2ConnectionWatchdog(0ms, 1s, [](ShutdownType, std::optional<int32_t>) {}), "C2 connection timeout must be greater than 0");
  }

  SECTION("Zero check period value") {
    REQUIRE_THROWS_WITH(c2::C2ConnectionWatchdog(1s, 0ms, [](ShutdownType, std::optional<int32_t>) {}),
        "C2 connection timeout check period must be greater than 0");
  }

  SECTION("Negative timeout value") {
    REQUIRE_THROWS_WITH(c2::C2ConnectionWatchdog(-1ms, 1s, [](ShutdownType, std::optional<int32_t>) {}), "C2 connection timeout must be greater than 0");
  }

  SECTION("Negative check period value") {
    REQUIRE_THROWS_WITH(c2::C2ConnectionWatchdog(1s, -1ms, [](ShutdownType, std::optional<int32_t>) {}),
        "C2 connection timeout check period must be greater than 0");
  }

  SECTION("Invalid check period value") {
    LogTestController::getInstance().setWarn<c2::C2ConnectionWatchdog>();
    c2::C2ConnectionWatchdog watchdog(10ms, 50ms, [](ShutdownType, std::optional<int32_t>) {});
    REQUIRE(LogTestController::getInstance().contains("C2 connection timeout check period is greater than connection timeout"));
  }
}

TEST_CASE("Test shutdown when no C2 connection is present at all", "[c2watchdog]") {
  LogTestController::getInstance().reset();
  c2::C2ConnectionWatchdog watchdog(400ms, 50ms, [&](ShutdownType shutdown_type, std::optional<int32_t> exit_code) {
    REQUIRE(shutdown_type == EXPECTED_SHUTDOWN_TYPE);
    REQUIRE(exit_code.has_value());
    REQUIRE(exit_code.value() == EXPECTED_EXIT_CODE);
  });
  REQUIRE(LogTestController::getInstance().contains("C2 connection timeout exceeded, shutting down...", 2s));
}

TEST_CASE("Test shutdown when C2 connection is lost", "[c2watchdog]") {
  LogTestController::getInstance().reset();
  c2::C2ConnectionWatchdog watchdog(400ms, 50ms, [&](ShutdownType shutdown_type, std::optional<int32_t> exit_code) {
    REQUIRE(shutdown_type == EXPECTED_SHUTDOWN_TYPE);
    REQUIRE(exit_code.has_value());
    REQUIRE(exit_code.value() == EXPECTED_EXIT_CODE);
  });
  for (int i = 0; i < 10; ++i) {
    watchdog.heartBeatHappened();
    REQUIRE_FALSE(LogTestController::getInstance().contains("C2 connection timeout exceeded, shutting down...", 0ms));
    std::this_thread::sleep_for(50ms);
  }
  REQUIRE(LogTestController::getInstance().contains("C2 connection timeout exceeded, shutting down...", 2s));
}

}  // namespace org::apache::nifi::minifi::test
