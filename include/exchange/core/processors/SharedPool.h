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

#include <cstdint>
#include <memory>
#include "../common/MatcherTradeEvent.h"

// Use moodycamel::ConcurrentQueue for lock-free high-performance queue
// Header-only library, faster than TBB concurrent_queue for high-frequency
// trading Note: moodycamel::ConcurrentQueue is unbounded, but performance is
// critical for HFT systems. poolMaxSize_ is kept for API compatibility.
#include <concurrentqueue.h>

namespace exchange::core::processors {

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
  common::MatcherTradeEvent* GetChain();

  /**
   * Offer next chain back to pool
   * Thread-safe (single producer safety is sufficient)
   * @param head - pointer to the first element of the chain
   */
  void PutChain(common::MatcherTradeEvent* head);

  /**
   * Get chain length
   */
  int32_t GetChainLength() const {
    return chainLength_;
  }

  /**
   * Delete an entire event chain (public for use when EVENTS_POOLING is false)
   * @param head Head of the chain to delete
   */
  static void DeleteChain(common::MatcherTradeEvent* head);

  /**
   * Destructor - cleans up all chains in the pool
   */
  ~SharedPool();

private:
  // Lock-free concurrent queue for high-performance event chain management
  // Replaces std::queue + std::mutex with lock-free implementation
  // Note: moodycamel::ConcurrentQueue is unbounded for maximum performance.
  // poolMaxSize_ is kept for API compatibility but not enforced (matches Java
  // behavior where offer() return value is ignored).
  moodycamel::ConcurrentQueue<common::MatcherTradeEvent*> eventChainsBuffer_;
  int32_t poolMaxSize_;  // Kept for API compatibility, not enforced
  int32_t chainLength_;
};

}  // namespace exchange::core::processors
