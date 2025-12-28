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
    : queueSize_(0), poolMaxSize_(poolMaxSize), chainLength_(chainLength) {
  if (poolInitialSize > poolMaxSize) {
    throw std::invalid_argument("too big poolInitialSize");
  }

  // Pre-generate initial chains
  for (int32_t i = 0; i < poolInitialSize; i++) {
    common::MatcherTradeEvent *chain =
        common::MatcherTradeEvent::CreateEventChain(chainLength);
    eventChainsBuffer_.enqueue(chain);
    queueSize_.fetch_add(1, std::memory_order_relaxed);
  }
}

common::MatcherTradeEvent *SharedPool::GetChain() {
  common::MatcherTradeEvent *head = nullptr;
  // Lock-free try_dequeue - much faster than mutex-based approach
  if (eventChainsBuffer_.try_dequeue(head)) {
    queueSize_.fetch_sub(1, std::memory_order_relaxed);
    return head;
  }
  // Pool is empty, create new chain
  return common::MatcherTradeEvent::CreateEventChain(chainLength_);
}

void SharedPool::PutChain(common::MatcherTradeEvent *head) {
  if (head == nullptr) {
    return;
  }

  // Match Java: LinkedBlockingQueue.offer() - bounded queue behavior
  // Check if queue is full before enqueueing
  int32_t currentSize = queueSize_.load(std::memory_order_relaxed);
  if (currentSize >= poolMaxSize_) {
    // Queue is full, delete chain instead of enqueueing (match Java behavior)
    // In Java, offer() returns false and chain is discarded (GC will reclaim)
    DeleteChain(head);
    return;
  }

  // Enqueue chain and update size counter
  eventChainsBuffer_.enqueue(head);
  queueSize_.fetch_add(1, std::memory_order_relaxed);
}

void SharedPool::DeleteChain(common::MatcherTradeEvent *head) {
  while (head != nullptr) {
    common::MatcherTradeEvent *next = head->nextEvent;
    delete head;
    head = next;
  }
}

SharedPool::~SharedPool() {
  // Dequeue and delete all chains remaining in the pool
  common::MatcherTradeEvent *chain = nullptr;
  while (eventChainsBuffer_.try_dequeue(chain)) {
    DeleteChain(chain);
    queueSize_.fetch_sub(1, std::memory_order_relaxed);
  }
}

} // namespace processors
} // namespace core
} // namespace exchange
