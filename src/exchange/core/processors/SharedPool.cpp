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

#include <exchange/core/common/MatcherTradeEvent.h>
#include <exchange/core/processors/SharedPool.h>
#include <stdexcept>

namespace exchange {
namespace core {
namespace processors {

std::unique_ptr<SharedPool> SharedPool::CreateTestSharedPool() {
  return std::make_unique<SharedPool>(8, 4, 256);
}

SharedPool::SharedPool(int32_t poolMaxSize, int32_t poolInitialSize,
                       int32_t chainLength)
    : poolMaxSize_(poolMaxSize), chainLength_(chainLength) {
  if (poolInitialSize > poolMaxSize) {
    throw std::invalid_argument("too big poolInitialSize");
  }

  // Pre-generate initial chains
  for (int32_t i = 0; i < poolInitialSize; i++) {
    common::MatcherTradeEvent *chain =
        common::MatcherTradeEvent::CreateEventChain(chainLength);
    eventChainsBuffer_.enqueue(chain);
  }
}

common::MatcherTradeEvent *SharedPool::GetChain() {
  common::MatcherTradeEvent *head = nullptr;
  // Lock-free try_dequeue - much faster than mutex-based approach
  if (eventChainsBuffer_.try_dequeue(head)) {
    return head;
  }
  // Pool is empty, create new chain
  return common::MatcherTradeEvent::CreateEventChain(chainLength_);
}

void SharedPool::PutChain(common::MatcherTradeEvent *head) {
  if (head == nullptr) {
    return;
  }

  // Lock-free enqueue - always succeeds (unbounded queue)
  // Note: Unlike Java LinkedBlockingQueue which is bounded,
  // moodycamel::ConcurrentQueue is unbounded. This is acceptable since object
  // pool is for optimization, not strict memory limiting. Performance is
  // critical for HFT systems - moodycamel::ConcurrentQueue is significantly
  // faster than TBB concurrent_bounded_queue.
  eventChainsBuffer_.enqueue(head);
}

} // namespace processors
} // namespace core
} // namespace exchange
