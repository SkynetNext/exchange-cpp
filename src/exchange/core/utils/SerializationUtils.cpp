/*
 * Copyright 2019 Maksim Zheravin
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <exchange/core/utils/SerializationUtils.h>

namespace exchange {
namespace core {
namespace utils {

ankerl::unordered_dense::map<int32_t, int64_t> SerializationUtils::MergeSum(
    const ankerl::unordered_dense::map<int32_t, int64_t> *map1,
    const ankerl::unordered_dense::map<int32_t, int64_t> *map2) {
  ankerl::unordered_dense::map<int32_t, int64_t> result;
  if (map1 != nullptr) {
    for (const auto &pair : *map1) {
      result[pair.first] = pair.second;
    }
  }
  if (map2 != nullptr) {
    for (const auto &pair : *map2) {
      result[pair.first] += pair.second;
    }
  }
  return result;
}

ankerl::unordered_dense::map<int32_t, int64_t> SerializationUtils::MergeSum(
    const ankerl::unordered_dense::map<int32_t, int64_t> *map1,
    const ankerl::unordered_dense::map<int32_t, int64_t> *map2,
    const ankerl::unordered_dense::map<int32_t, int64_t> *map3) {
  auto result = MergeSum(map1, map2);
  if (map3 != nullptr) {
    for (const auto &pair : *map3) {
      result[pair.first] += pair.second;
    }
  }
  return result;
}

ankerl::unordered_dense::map<int32_t, int64_t> SerializationUtils::MergeSum(
    const ankerl::unordered_dense::map<int32_t, int64_t> *map1,
    const ankerl::unordered_dense::map<int32_t, int64_t> *map2,
    const ankerl::unordered_dense::map<int32_t, int64_t> *map3,
    const ankerl::unordered_dense::map<int32_t, int64_t> *map4) {
  auto result = MergeSum(map1, map2, map3);
  if (map4 != nullptr) {
    for (const auto &pair : *map4) {
      result[pair.first] += pair.second;
    }
  }
  return result;
}

} // namespace utils
} // namespace core
} // namespace exchange
