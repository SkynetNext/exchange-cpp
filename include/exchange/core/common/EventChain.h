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

#include "MatcherTradeEvent.h"
#include <cstdint>

namespace exchange {
namespace core {
namespace common {

/**
 * @brief RAII wrapper for MatcherTradeEvent chain management
 *
 * This class provides safe, exception-safe management of event chains
 * following modern C++ best practices:
 * - RAII: automatic resource management
 * - Move semantics: efficient transfer of ownership
 * - Clear ownership model: single owner at a time
 * - Exception safety: guarantees cleanup even on exceptions
 *
 * Usage:
 *   // Create a chain
 *   EventChain chain(MatcherTradeEvent::CreateEventChain(10));
 *
 *   // Access head
 *   MatcherTradeEvent* head = chain.Head();
 *
 *   // Transfer ownership (move)
 *   EventChain other = std::move(chain);
 *
 *   // Release ownership (caller responsible for cleanup)
 *   MatcherTradeEvent* released = chain.Release();
 */
class EventChain {
public:
  /**
   * @brief Construct from raw pointer (takes ownership)
   * @param head Head of the event chain, or nullptr
   *
   * @note Takes ownership of the chain. If head is nullptr, chain is empty.
   */
  explicit EventChain(MatcherTradeEvent *head = nullptr) noexcept
      : head_(head) {}

  /**
   * @brief Non-copyable (single ownership)
   */
  EventChain(const EventChain &) = delete;
  EventChain &operator=(const EventChain &) = delete;

  /**
   * @brief Move constructor
   */
  EventChain(EventChain &&other) noexcept : head_(other.head_) {
    other.head_ = nullptr;
  }

  /**
   * @brief Move assignment
   */
  EventChain &operator=(EventChain &&other) noexcept {
    if (this != &other) {
      DeleteChain();
      head_ = other.head_;
      other.head_ = nullptr;
    }
    return *this;
  }

  /**
   * @brief Destructor - automatically deletes the chain
   */
  ~EventChain() { DeleteChain(); }

  /**
   * @brief Get the head of the chain (non-owning pointer)
   * @return Head of the chain, or nullptr if empty
   */
  MatcherTradeEvent *Head() const noexcept { return head_; }

  /**
   * @brief Check if chain is empty
   */
  bool IsEmpty() const noexcept { return head_ == nullptr; }

  /**
   * @brief Get chain size (number of events)
   */
  int32_t Size() const {
    if (head_ == nullptr) {
      return 0;
    }
    return head_->GetChainSize();
  }

  /**
   * @brief Release ownership of the chain
   * @return Head of the chain, or nullptr if empty
   *
   * @note After release, this object no longer owns the chain.
   *       Caller is responsible for cleanup.
   */
  MatcherTradeEvent *Release() noexcept {
    MatcherTradeEvent *result = head_;
    head_ = nullptr;
    return result;
  }

  /**
   * @brief Reset the chain (delete current and take ownership of new)
   * @param newHead New chain head, or nullptr
   */
  void Reset(MatcherTradeEvent *newHead = nullptr) noexcept {
    DeleteChain();
    head_ = newHead;
  }

  /**
   * @brief Append another chain to the end of this chain
   * @param other Chain to append (will be released from other)
   *
   * @note After append, other is empty (ownership transferred)
   */
  void Append(EventChain &&other) {
    if (other.IsEmpty()) {
      return;
    }
    if (IsEmpty()) {
      head_ = other.Release();
      return;
    }

    // Find tail of current chain
    MatcherTradeEvent *tail = head_->FindTail();
    // Append other chain
    tail->nextEvent = other.Release();
  }

  /**
   * @brief Static helper to delete a chain (for compatibility)
   * @param head Head of chain to delete, or nullptr
   */
  static void Delete(MatcherTradeEvent *head) {
    while (head != nullptr) {
      MatcherTradeEvent *next = head->nextEvent;
      delete head;
      head = next;
    }
  }

private:
  MatcherTradeEvent *head_;

  /**
   * @brief Delete the current chain
   */
  void DeleteChain() {
    if (head_ != nullptr) {
      Delete(head_);
      head_ = nullptr;
    }
  }
};

} // namespace common
} // namespace core
} // namespace exchange
