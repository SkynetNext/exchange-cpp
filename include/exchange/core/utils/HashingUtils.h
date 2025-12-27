/*
 * Copyright 2025 Justin Zhu
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
#include <functional>
#include <vector>

namespace exchange {
namespace core {
namespace utils {

/**
 * HashingUtils - utility functions for state hashing
 */
class HashingUtils {
public:
  /**
   * Calculate state hash for a hash map of StateHash objects
   */
  template <typename T>
  static int32_t
  StateHash(const ankerl::unordered_dense::map<int64_t, T *> &hashMap) {
    std::size_t hash = 0;
    for (const auto &pair : hashMap) {
      std::size_t h1 = std::hash<int64_t>{}(pair.first);
      if (pair.second != nullptr) {
        h1 ^= static_cast<std::size_t>(pair.second->GetStateHash());
      }
      hash ^= (h1 << 1);
    }
    return static_cast<int32_t>(hash);
  }

  template <typename T>
  static int32_t
  StateHash(const ankerl::unordered_dense::map<int32_t, T *> &hashMap) {
    std::size_t hash = 0;
    for (const auto &pair : hashMap) {
      std::size_t h1 = std::hash<int32_t>{}(pair.first);
      if (pair.second != nullptr) {
        h1 ^= static_cast<std::size_t>(pair.second->GetStateHash());
      }
      hash ^= (h1 << 1);
    }
    return static_cast<int32_t>(hash);
  }

  /**
   * Calculate state hash for a vector of StateHash objects
   */
  template <typename T>
  static int32_t StateHashStream(const std::vector<T *> &items) {
    int32_t h = 0;
    for (const auto *item : items) {
      if (item != nullptr) {
        h = h * 31 + item->GetStateHash();
      }
    }
    return h;
  }
};

} // namespace utils
} // namespace core
} // namespace exchange
