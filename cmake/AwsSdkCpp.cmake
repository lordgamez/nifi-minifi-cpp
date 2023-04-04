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
set(PATCH_FILE1 "${CMAKE_SOURCE_DIR}/thirdparty/aws-sdk-cpp/c++20-compilation-fixes.patch")
set(PATCH_FILE2 "${CMAKE_SOURCE_DIR}/thirdparty/aws-sdk-cpp/dll-export-injection.patch")
set(PATCH_FILE3 "${CMAKE_SOURCE_DIR}/thirdparty/aws-sdk-cpp/shutdown-fix.patch")
set(PATCH_FILE4 "${CMAKE_SOURCE_DIR}/thirdparty/aws-sdk-cpp/bundle-openssl.patch")
set(AWS_SDK_CPP_PATCH_COMMAND ${Bash_EXECUTABLE} -c "set -x &&\
        (\\\"${Patch_EXECUTABLE}\\\" -p1 -R -s -f --dry-run -i \\\"${PATCH_FILE1}\\\" || \\\"${Patch_EXECUTABLE}\\\" -p1 -N -i \\\"${PATCH_FILE1}\\\") &&\
        (\\\"${Patch_EXECUTABLE}\\\" -p1 -R -s -f --dry-run -i \\\"${PATCH_FILE2}\\\" || \\\"${Patch_EXECUTABLE}\\\" -p1 -N -i \\\"${PATCH_FILE2}\\\") &&\
        (\\\"${Patch_EXECUTABLE}\\\" -p1 -R -s -f --dry-run -i \\\"${PATCH_FILE3}\\\" || \\\"${Patch_EXECUTABLE}\\\" -p1 -N -i \\\"${PATCH_FILE3}\\\") &&\
        (\\\"${Patch_EXECUTABLE}\\\" -p1 -R -s -f --dry-run -i \\\"${PATCH_FILE4}\\\" || \\\"${Patch_EXECUTABLE}\\\" -p1 -N -i \\\"${PATCH_FILE4}\\\") ")

set(BUILD_ONLY "s3" CACHE STRING "" FORCE)
set(ENABLE_TESTING OFF CACHE BOOL "" FORCE)
set(BUILD_TESTING OFF CACHE BOOL "" FORCE)
set(AUTORUN_UNIT_TESTS OFF CACHE BOOL "" FORCE)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(ENABLE_UNITY_BUILD ${AWS_ENABLE_UNITY_BUILD} CACHE BOOL "" FORCE)
if(WIN32)
    set(FORCE_EXPORT_CORE_API ON CACHE BOOL "" FORCE)
    set(FORCE_EXPORT_S3_API ON CACHE BOOL "" FORCE)

    # # https://github.com/aws/aws-sdk-cpp/issues/20
    # if(NOT CMAKE_CXX_FLAGS_DEBUGOPT)
    #     set(CMAKE_CXX_FLAGS_DEBUGOPT "")
    # endif()

    # if(NOT CMAKE_EXE_LINKER_FLAGS_DEBUGOPT)
    #     set(CMAKE_EXE_LINKER_FLAGS_DEBUGOPT "")
    # endif()

    # if(NOT CMAKE_SHARED_LINKER_FLAGS_DEBUGOPT)
    #     set(CMAKE_SHARED_LINKER_FLAGS_DEBUGOPT "")
    # endif()
endif()

FetchContent_Declare(aws-sdk-cpp
    GIT_REPOSITORY "https://github.com/aws/aws-sdk-cpp.git"
    GIT_TAG "1.11.51"
    GIT_PROGRESS TRUE
    PATCH_COMMAND "${AWS_SDK_CPP_PATCH_COMMAND}"
)
FetchContent_MakeAvailable(aws-sdk-cpp)

add_dependencies(aws-cpp-sdk-s3 CURL::libcurl OpenSSL::Crypto OpenSSL::SSL ZLIB::ZLIB)
target_compile_options(aws-crt-cpp PRIVATE -Wno-error)
target_compile_options(aws-cpp-sdk-s3 PRIVATE -Wno-error)
