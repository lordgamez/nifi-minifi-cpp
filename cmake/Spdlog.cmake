#
# Licensed to the Apache Software Foundation (ASF) under one
# or more contributor license agreements.  See the NOTICE file
# distributed with this work for additional information
# regarding copyright ownership.  The ASF licenses this file
# to you under the Apache License, Version 2.0 (the
# "License"); you may not use this file except in compliance
# with the License.  You may obtain a copy of the License at
#
#   http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing,
# software distributed under the License is distributed on an
# "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
# KIND, either express or implied.  See the License for the
# specific language governing permissions and limitations
# under the License.
#
include(FetchContent)

set(SPDLOG_FMT_EXTERNAL ON CACHE STRING "" FORCE)
set(SPDLOG_SYSTEM_INCLUDES ON CACHE STRING "" FORCE)

FetchContent_Declare(Spdlog
        URL  https://github.com/gabime/spdlog/archive/refs/tags/v1.16.0.tar.gz
        URL_HASH SHA256=8741753e488a78dd0d0024c980e1fb5b5c85888447e309d9cb9d949bdb52aa3e
        OVERRIDE_FIND_PACKAGE
        SYSTEM
        )
FetchContent_MakeAvailable(Spdlog)
