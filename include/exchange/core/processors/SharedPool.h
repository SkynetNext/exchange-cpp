/*
 * Copyright 2020 Maksim Zheravin
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

#include "../common/MatcherTradeEvent.h"
#include <cstdint>
#include <memory>

// Use TBB concurrent_bounded_queue for bounded lock-free queue
// Matches Java LinkedBlockingQueue behavior (bounded, try_push returns false
// when full)
#include <tbb/concurrent_queue.h>

namespace exchange {
namespace core {
namespace processors {

/**
 * SharedPool - manages event chains for MatcherTradeEvent
 *
 * Thread-safe pool for reusing MatcherTradeEvent chains to reduce allocations
 */
class SharedPool {
public:
  /**
   * Create a test shared pool with default settings
   */
  static std::unique_ptr<SharedPool> CreateTestSharedPool();

  /**
   * Create new shared pool
   * @param poolMaxSize - max size of pool. Will skip new chains if chains
   * buffer is full.
   * @param poolInitialSize - initial number of pre-generated chains.
   * Recommended to set higher than number of modules - (RE+ME)*2.
   * @param chainLength - target chain length. Longer chain means rare requests
   * for new chains. However longer chains can cause event placeholders
   * starvation.
   */
  SharedPool(int32_t poolMaxSize, int32_t poolInitialSize, int32_t chainLength);

  /**
   * Request next chain from buffer
   * Thread-safe
   * @return chain head, or creates new chain if pool is empty
   */
  common::MatcherTradeEvent *GetChain();

  /**
   * Offer next chain back to pool
   * Thread-safe (single producer safety is sufficient)
   * @param head - pointer to the first element of the chain
   */
  void PutChain(common::MatcherTradeEvent *head);

  /**
   * Get chain length
   */
  int32_t GetChainLength() const { return chainLength_; }

private:
  // Bounded lock-free concurrent queue for high-performance event chain
  // management Matches Java LinkedBlockingQueue behavior: try_push returns
  // false when queue is full
  tbb::concurrent_bounded_queue<common::MatcherTradeEvent *> eventChainsBuffer_;
  int32_t poolMaxSize_; // Enforced by concurrent_bounded_queue capacity
  int32_t chainLength_;
};

} // namespace processors
} // namespace core
} // namespace exchange
