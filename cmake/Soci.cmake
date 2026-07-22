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

include(FetchContent)

include(fmt)

set(PATCH_FILE1 "${CMAKE_SOURCE_DIR}/thirdparty/soci/patches/relax-sqlwchar-static-assert.patch")
set(PATCH_FILE2 "${CMAKE_SOURCE_DIR}/thirdparty/soci/patches/odbc-map-wide-columns-to-string.patch")
set(PC ${Bash_EXECUTABLE} -c "set -x &&\
        (\\\"${Patch_EXECUTABLE}\\\" -p1 -R -s -f --dry-run -i \\\"${PATCH_FILE1}\\\" || \\\"${Patch_EXECUTABLE}\\\" -p1 -N -i \\\"${PATCH_FILE1}\\\") &&\
        (\\\"${Patch_EXECUTABLE}\\\" -p1 -R -s -f --dry-run -i \\\"${PATCH_FILE2}\\\" || \\\"${Patch_EXECUTABLE}\\\" -p1 -N -i \\\"${PATCH_FILE2}\\\")")

if(WIN32)
    set(SOCI_TESTS OFF CACHE BOOL "" FORCE)
    set(SOCI_SHARED OFF CACHE BOOL "" FORCE)
    set(WITH_ODBC ON CACHE BOOL "" FORCE)
    set(WITH_BOOST OFF CACHE BOOL "" FORCE)
else()
    set(SOCI_TESTS OFF CACHE BOOL "" FORCE)
    set(SOCI_SHARED OFF CACHE BOOL "" FORCE)
    set(SOCI_ODBC ON CACHE BOOL "" FORCE)
    set(WITH_BOOST OFF CACHE BOOL "" FORCE)
endif()

FetchContent_Declare(
    soci
    GIT_REPOSITORY "https://github.com/SOCI/soci.git"
    GIT_TAG "v4.1.4"
    GIT_SUBMODULES "3rdparty/sqlite3"
    PATCH_COMMAND "${PC}"
    SYSTEM
)

FetchContent_MakeAvailable(soci)

if(NOT WIN32)
    add_dependencies(soci_core ODBC::ODBC)
endif()
