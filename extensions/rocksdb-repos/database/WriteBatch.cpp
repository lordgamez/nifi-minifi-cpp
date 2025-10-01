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

#include "WriteBatch.h"

namespace org::apache::nifi::minifi::internal {

rocksdb::Status WriteBatch::Put(const rocksdb::Slice &key, const rocksdb::Slice &value) {
  return impl_.Put(column_, key, value);
}

rocksdb::Status WriteBatch::Delete(const rocksdb::Slice &key) {
  return impl_.Delete(column_, key);
}

rocksdb::Status WriteBatch::Merge(const rocksdb::Slice &key, const rocksdb::Slice &value) {
  return impl_.Merge(column_, key, value);
}

}  // namespace org::apache::nifi::minifi::internal
