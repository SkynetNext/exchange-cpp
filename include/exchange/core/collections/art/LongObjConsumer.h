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

#include <cstdint>

namespace exchange::core::collections::art {

/**
 * LongObjConsumer - functional interface for consuming key-value pairs
 * @tparam V Value type
 */
template <typename V>
class LongObjConsumer {
public:
  virtual ~LongObjConsumer() = default;

  /**
   * Performs this operation on the given arguments
   * @param key 64-bit key
   * @param value Value
   */
  virtual void Accept(int64_t key, V* value) = 0;
};

/**
 * LambdaConsumer - utility to wrap lambda as LongObjConsumer
 */
template <typename V, typename F>
class LambdaConsumer : public LongObjConsumer<V> {
public:
  explicit LambdaConsumer(F f) : f_(f) {}

  void Accept(int64_t key, V* value) override {
    f_(key, value);
  }

private:
  F f_;
};

}  // namespace exchange::core::collections::art
