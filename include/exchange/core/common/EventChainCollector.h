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

#include "EventChain.h"
#include "MatcherTradeEvent.h"
#include <cstdint>
#include <functional>

namespace exchange {
namespace core {
namespace common {

/**
 * @brief Collects event chains and manages their lifecycle
 *
 * This class provides a clean interface for collecting event chains
 * and returning them to a pool when a target size is reached.
 *
 * Best practices:
 * - Clear ownership: owns collected chains until returned to pool
 * - Exception safe: guarantees cleanup on destruction
 * - Simple interface: easy to use correctly
 *
 * Usage:
 *   EventChainCollector collector(pool, targetLength);
 *
 *   // Collect events
 *   collector.Collect(cmd->matcherEvent);
 *   cmd->matcherEvent = nullptr;
 *
 *   // Check if ready to return
 *   if (collector.ShouldReturn()) {
 *     collector.ReturnToPool();
 *   }
 *
 *   // On group change or shutdown
 *   collector.Flush(); // Return any remaining chains
 */
class EventChainCollector {
public:
  using PoolCallback = std::function<void(MatcherTradeEvent *)>;

  /**
   * @brief Construct collector
   * @param poolCallback Callback to return chain to pool (can be nullptr)
   * @param targetLength Target chain length before returning to pool
   *
   * @note If poolCallback is nullptr, chains will be deleted instead
   */
  explicit EventChainCollector(PoolCallback poolCallback = nullptr,
                               int32_t targetLength = 0) noexcept
      : poolCallback_(std::move(poolCallback)),
        targetLength_(targetLength > 0 ? targetLength : 0), currentSize_(0) {}

  /**
   * @brief Non-copyable
   */
  EventChainCollector(const EventChainCollector &) = delete;
  EventChainCollector &operator=(const EventChainCollector &) = delete;

  /**
   * @brief Move constructor
   */
  EventChainCollector(EventChainCollector &&other) noexcept
      : poolCallback_(std::move(other.poolCallback_)),
        targetLength_(other.targetLength_), currentSize_(other.currentSize_),
        head_(std::move(other.head_)), tail_(other.tail_) {
    other.targetLength_ = 0;
    other.currentSize_ = 0;
    other.tail_ = nullptr;
  }

  /**
   * @brief Move assignment
   */
  EventChainCollector &operator=(EventChainCollector &&other) noexcept {
    if (this != &other) {
      Flush();
      poolCallback_ = std::move(other.poolCallback_);
      targetLength_ = other.targetLength_;
      currentSize_ = other.currentSize_;
      head_ = std::move(other.head_);
      tail_ = other.tail_;

      other.targetLength_ = 0;
      other.currentSize_ = 0;
      other.tail_ = nullptr;
    }
    return *this;
  }

  /**
   * @brief Destructor - flushes any remaining chains
   */
  ~EventChainCollector() { Flush(); }

  /**
   * @brief Collect an event chain
   * @param chainHead Head of chain to collect, or nullptr
   *
   * @note Takes ownership of the chain. After call, chainHead should be set to
   * nullptr.
   */
  void Collect(MatcherTradeEvent *chainHead) {
    if (chainHead == nullptr) {
      return;
    }

    // Calculate chain size
    int32_t chainSize = 1;
    MatcherTradeEvent *chainTail = chainHead;
    while (chainTail->nextEvent != nullptr) {
      chainTail = chainTail->nextEvent;
      chainSize++;
    }

    // Append to collected chain
    if (head_.IsEmpty()) {
      head_.Reset(chainHead);
      tail_ = chainTail;
    } else {
      tail_->nextEvent = chainHead;
      tail_ = chainTail;
    }

    currentSize_ += chainSize;
  }

  /**
   * @brief Check if should return chain to pool
   */
  bool ShouldReturn() const noexcept {
    return targetLength_ > 0 && currentSize_ >= targetLength_ &&
           !head_.IsEmpty();
  }

  /**
   * @brief Return collected chain to pool (if ready)
   * @return true if chain was returned, false otherwise
   */
  bool ReturnToPool() {
    if (!ShouldReturn()) {
      return false;
    }

    MatcherTradeEvent *chain = head_.Release();
    if (poolCallback_ && chain != nullptr) {
      poolCallback_(chain);
    } else {
      EventChain::Delete(chain);
    }

    tail_ = nullptr;
    currentSize_ = 0;
    return true;
  }

  /**
   * @brief Flush any remaining chains (return to pool or delete)
   */
  void Flush() {
    if (head_.IsEmpty()) {
      return;
    }

    MatcherTradeEvent *chain = head_.Release();
    if (poolCallback_ && chain != nullptr) {
      poolCallback_(chain);
    } else {
      EventChain::Delete(chain);
    }

    tail_ = nullptr;
    currentSize_ = 0;
  }

  /**
   * @brief Get current collected chain size
   */
  int32_t CurrentSize() const noexcept { return currentSize_; }

  /**
   * @brief Check if collector is empty
   */
  bool IsEmpty() const noexcept { return head_.IsEmpty(); }

private:
  PoolCallback poolCallback_;
  int32_t targetLength_;
  int32_t currentSize_;
  EventChain head_;
  MatcherTradeEvent *tail_ = nullptr;
};

} // namespace common
} // namespace core
} // namespace exchange
