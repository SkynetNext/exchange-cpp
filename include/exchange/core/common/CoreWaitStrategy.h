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

namespace exchange {
namespace core {
namespace common {

/**
 * Wait strategy types for Disruptor
 * Maps to disruptor-cpp wait strategies
 */
enum class CoreWaitStrategy : uint8_t {
  BUSY_SPIN = 0,           // BusySpinWaitStrategy - lowest latency, highest CPU
  YIELDING = 1,            // YieldingWaitStrategy - moderate latency, moderate CPU
  BLOCKING = 2,            // BlockingWaitStrategy - higher latency, lower CPU
  SECOND_STEP_NO_WAIT = 3  // special case - no wait for second step
};

inline bool ShouldYield(CoreWaitStrategy strategy) {
  return strategy == CoreWaitStrategy::YIELDING;
}

inline bool ShouldBlock(CoreWaitStrategy strategy) {
  return strategy == CoreWaitStrategy::BLOCKING;
}

inline bool IsNoWait(CoreWaitStrategy strategy) {
  return strategy == CoreWaitStrategy::SECOND_STEP_NO_WAIT;
}

}  // namespace common
}  // namespace core
}  // namespace exchange
