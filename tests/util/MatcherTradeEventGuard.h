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

#include <exchange/core/common/MatcherTradeEvent.h>
#include <exchange/core/common/cmd/OrderCommand.h>

namespace exchange::core::tests::util {

/**
 * RAII guard for MatcherTradeEvent chain cleanup.
 * Automatically deletes the event chain when the guard goes out of scope.
 *
 * Usage:
 *   OrderCommand cmd = ...;
 *   ProcessAndValidate(cmd, ...);
 *   MatcherTradeEventGuard guard(cmd);  // Takes ownership
 *   auto events = cmd.ExtractEvents();
 *   // ... verify events ...
 *   // guard automatically deletes chain on scope exit
 */
class MatcherTradeEventGuard {
public:
  /**
   * Construct from OrderCommand reference.
   * Takes ownership of cmd.matcherEvent and clears it.
   */
  explicit MatcherTradeEventGuard(common::cmd::OrderCommand& cmd) : eventChain_(cmd.matcherEvent) {
    cmd.matcherEvent = nullptr;
  }

  /**
   * Construct from raw MatcherTradeEvent pointer.
   * Takes ownership of the event chain.
   */
  explicit MatcherTradeEventGuard(common::MatcherTradeEvent* eventChain)
    : eventChain_(eventChain) {}

  // Non-copyable
  MatcherTradeEventGuard(const MatcherTradeEventGuard&) = delete;
  MatcherTradeEventGuard& operator=(const MatcherTradeEventGuard&) = delete;

  // Movable
  MatcherTradeEventGuard(MatcherTradeEventGuard&& other) noexcept : eventChain_(other.eventChain_) {
    other.eventChain_ = nullptr;
  }

  MatcherTradeEventGuard& operator=(MatcherTradeEventGuard&& other) noexcept {
    if (this != &other) {
      // Delete current chain if any
      common::MatcherTradeEvent::DeleteChain(eventChain_);
      eventChain_ = other.eventChain_;
      other.eventChain_ = nullptr;
    }
    return *this;
  }

  /**
   * Destructor - automatically deletes the event chain.
   */
  ~MatcherTradeEventGuard() {
    common::MatcherTradeEvent::DeleteChain(eventChain_);
  }

  /**
   * Get the event chain (for verification purposes).
   * The chain will still be deleted when the guard is destroyed.
   */
  common::MatcherTradeEvent* Get() const noexcept {
    return eventChain_;
  }

  /**
   * Release ownership of the event chain.
   * Returns the chain pointer and clears the guard.
   * Caller is responsible for cleanup after this call.
   */
  common::MatcherTradeEvent* Release() noexcept {
    common::MatcherTradeEvent* chain = eventChain_;
    eventChain_ = nullptr;
    return chain;
  }

private:
  common::MatcherTradeEvent* eventChain_;
};

}  // namespace exchange::core::tests::util
