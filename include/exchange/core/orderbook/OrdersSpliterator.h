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

#include <cstdint>
#include <functional>

namespace exchange {
namespace core {
namespace orderbook {

// Forward declaration
class OrderBookDirectImpl;

/**
 * OrdersSpliterator - iterator for orders in OrderBookDirectImpl
 * C++ equivalent of Java Spliterator
 */
class OrdersSpliterator {
public:
  using DirectOrder =
      void *; // Will be properly typed when OrderBookDirectImpl is complete

  explicit OrdersSpliterator(DirectOrder pointer) : pointer_(pointer) {}

  /**
   * Try to advance and call action
   */
  bool TryAdvance(std::function<void(DirectOrder)> action);

  /**
   * Estimate size (always returns max for simplicity)
   */
  int64_t EstimateSize() const { return INT64_MAX; }

  /**
   * Characteristics (ordered)
   */
  int32_t Characteristics() const { return 1; } // ORDERED

private:
  DirectOrder pointer_;
};

} // namespace orderbook
} // namespace core
} // namespace exchange
