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

function(use_bundled_lmdb SOURCE_DIR BINARY_DIR)
    if (WIN32)
        set(BYPRODUCT "lmdb.lib")
    else()
        set(BYPRODUCT "liblmdb.a")
    endif()

    processorcount(jobs)

    # Build project
    ExternalProject_Add(
            lmdb-external
            URL "https://github.com/LMDB/lmdb/archive/refs/tags/LMDB_1.0.0-branch.tar.gz"
            URL_HASH "SHA256=8d3e790194e43a72f172f34c442ea4737b2d1433fc0983f2ef70bae999bc2d28"
            SOURCE_DIR "${BINARY_DIR}/thirdparty/lmdb-src/"
            SOURCE_SUBDIR "libraries/liblmdb"
            BUILD_BYPRODUCTS "${BINARY_DIR}/thirdparty/lmdb-src/libraries/liblmdb/${BYPRODUCT}"
            EXCLUDE_FROM_ALL TRUE
            LIST_SEPARATOR % # This is needed for passing semicolon-separated lists
            DOWNLOAD_NO_PROGRESS TRUE
            TLS_VERIFY TRUE
            BUILD_IN_SOURCE TRUE
            CONFIGURE_COMMAND ""
            BUILD_COMMAND make -j${jobs} CFLAGS=-fPIC
            INSTALL_COMMAND ""
    )

    # Set variables
    set(LMDB_FOUND "YES" CACHE STRING "" FORCE)
    set(LMDB_INCLUDE_DIR "${BINARY_DIR}/thirdparty/lmdb-src/libraries/liblmdb" CACHE STRING "" FORCE)
    set(LMDB_LIBRARY "${BINARY_DIR}/thirdparty/lmdb-src/libraries/liblmdb/${BYPRODUCT}" CACHE STRING "" FORCE)

    # Create imported targets
    add_library(LMDB::LMDB STATIC IMPORTED)
    set_target_properties(LMDB::LMDB PROPERTIES IMPORTED_LOCATION "${LMDB_LIBRARY}")
    add_dependencies(LMDB::LMDB lmdb-external)
endfunction(use_bundled_lmdb)
