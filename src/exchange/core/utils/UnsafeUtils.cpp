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

#include <atomic>
#include <exchange/core/common/cmd/OrderCommand.h>
#include <exchange/core/utils/UnsafeUtils.h>

namespace exchange {
namespace core {
namespace utils {

void UnsafeUtils::SetResultVolatile(
    common::cmd::OrderCommand *cmd, bool result,
    common::cmd::CommandResultCode successCode,
    common::cmd::CommandResultCode failureCode) {
  common::cmd::CommandResultCode codeToSet = result ? successCode : failureCode;

  // Use atomic compare-and-swap
  common::cmd::CommandResultCode expected = cmd->resultCode;
  while (expected != codeToSet && expected != failureCode) {
    if (std::atomic_compare_exchange_weak(
            reinterpret_cast<std::atomic<common::cmd::CommandResultCode> *>(
                &cmd->resultCode),
            &expected, codeToSet)) {
      break;
    }
  }
}

void UnsafeUtils::AppendEventsVolatile(common::cmd::OrderCommand *cmd,
                                       common::MatcherTradeEvent *eventHead) {
  if (eventHead == nullptr) {
    return;
  }

  common::MatcherTradeEvent *tail = eventHead->FindTail();

  // Use atomic compare-and-swap to append events
  common::MatcherTradeEvent *expected = cmd->matcherEvent;
  do {
    tail->nextEvent = expected;
  } while (!std::atomic_compare_exchange_weak(
      reinterpret_cast<std::atomic<common::MatcherTradeEvent *> *>(
          &cmd->matcherEvent),
      &expected, eventHead));
}

} // namespace utils
} // namespace core
} // namespace exchange
