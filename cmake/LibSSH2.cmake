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

set(ENABLE_ZLIB_COMPRESSION ON CACHE BOOL "" FORCE)
set(CRYPTO_BACKEND "OpenSSL" CACHE STRING "" FORCE)
set(BUILD_TESTING OFF CACHE BOOL "" FORCE)
set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)

set(PATCH_FILE "${CMAKE_SOURCE_DIR}/thirdparty/libssh2/libssh2-CMAKE_MODULE_PATH.patch")
set(PC ${Bash_EXECUTABLE}  -c "set -x &&\
        (\\\"${Patch_EXECUTABLE}\\\" -p1 -R -s -f --dry-run -i \\\"${PATCH_FILE}\\\" || \\\"${Patch_EXECUTABLE}\\\" -p1 -N -i \\\"${PATCH_FILE}\\\")")

FetchContent_Declare(
    libssh2
    URL "https://github.com/libssh2/libssh2/archive/refs/tags/libssh2-1.11.1.tar.gz"
    URL_HASH "SHA256=82b35c61c78b475647bdc981a183c5b5ab0d979e1caee94186e8f9150f2b0d0d"
    PATCH_COMMAND "${PC}"
    SYSTEM
)

FetchContent_MakeAvailable(libssh2)

add_dependencies(libssh2_static OpenSSL::Crypto ZLIB::ZLIB)

if (WIN32)
    set(LIBSSH2_BYPRODUCT_SUFFIX ".lib" CACHE STRING "" FORCE)
else()
    set(LIBSSH2_BYPRODUCT_SUFFIX ".a" CACHE STRING "" FORCE)
endif()

# Set variables
set(LIBSSH2_FOUND "YES" CACHE STRING "" FORCE)
set(LIBSSH2_INCLUDE_DIR "${libssh2_BINARY_DIR}/include" CACHE STRING "" FORCE)
set(LIBSSH2_LIBRARY "${libssh2_BINARY_DIR}/libssh2.${LIBSSH2_BYPRODUCT_SUFFIX}" CACHE STRING "" FORCE)

# Set exported variables for FindPackage.cmake
set(PASSTHROUGH_VARIABLES ${PASSTHROUGH_VARIABLES} "-DEXPORTED_LIBSSH2_INCLUDE_DIR=${LIBSSH2_INCLUDE_DIR}" CACHE STRING "" FORCE)
set(PASSTHROUGH_VARIABLES ${PASSTHROUGH_VARIABLES} "-DEXPORTED_LIBSSH2_LIBRARY=${LIBSSH2_LIBRARY}" CACHE STRING "" FORCE)
