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

#pragma once

#include <ankerl/unordered_dense.h>
#include <cstdint>

namespace exchange {
namespace core {
namespace utils {

/**
 * SerializationUtils - serialization utility functions
 * Simplified version for now
 */
class SerializationUtils {
public:
  /**
   * Merge sum of two maps (currency -> amount)
   */
  static ankerl::unordered_dense::map<int32_t, int64_t>
  MergeSum(const ankerl::unordered_dense::map<int32_t, int64_t> *map1,
           const ankerl::unordered_dense::map<int32_t, int64_t> *map2);

  /**
   * Merge sum of multiple maps
   */
  static ankerl::unordered_dense::map<int32_t, int64_t>
  MergeSum(const ankerl::unordered_dense::map<int32_t, int64_t> *map1,
           const ankerl::unordered_dense::map<int32_t, int64_t> *map2,
           const ankerl::unordered_dense::map<int32_t, int64_t> *map3);

  /**
   * Merge sum of multiple maps (4 maps)
   */
  static ankerl::unordered_dense::map<int32_t, int64_t>
  MergeSum(const ankerl::unordered_dense::map<int32_t, int64_t> *map1,
           const ankerl::unordered_dense::map<int32_t, int64_t> *map2,
           const ankerl::unordered_dense::map<int32_t, int64_t> *map3,
           const ankerl::unordered_dense::map<int32_t, int64_t> *map4);

  /**
   * Prefer non-null value
   */
  template <typename T> static T *PreferNotNull(T *a, T *b) {
    return a != nullptr ? a : b;
  }

  /**
   * Merge override - merge two maps, b overrides a
   */
  template <typename K, typename V>
  static ankerl::unordered_dense::map<K, V>
  MergeOverride(const ankerl::unordered_dense::map<K, V> *a,
                const ankerl::unordered_dense::map<K, V> *b) {
    ankerl::unordered_dense::map<K, V> result;
    if (a != nullptr) {
      result = *a;
    }
    if (b != nullptr) {
      for (const auto &pair : *b) {
        result[pair.first] = pair.second;
      }
    }
    return result;
  }
};

} // namespace utils
} // namespace core
} // namespace exchange
