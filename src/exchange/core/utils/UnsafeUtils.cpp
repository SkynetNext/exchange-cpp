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

#include <exchange/core/common/cmd/OrderCommand.h>
#include <exchange/core/utils/UnsafeUtils.h>
#include <atomic>


namespace exchange {
namespace core {
namespace utils {

void UnsafeUtils::SetResultVolatile(common::cmd::OrderCommand* cmd,
                                    bool result,
                                    common::cmd::CommandResultCode successCode,
                                    common::cmd::CommandResultCode failureCode) {
    common::cmd::CommandResultCode codeToSet = result ? successCode : failureCode;

    // Use atomic compare-and-swap
    // Initial load must also be atomic to avoid data race with concurrent CAS
    auto* atomicPtr =
        reinterpret_cast<std::atomic<common::cmd::CommandResultCode>*>(&cmd->resultCode);
    common::cmd::CommandResultCode expected = atomicPtr->load(std::memory_order_relaxed);
    while (expected != codeToSet && expected != failureCode) {
        if (atomicPtr->compare_exchange_weak(
                expected, codeToSet, std::memory_order_release, std::memory_order_relaxed)) {
            break;
        }
    }
}

void UnsafeUtils::AppendEventsVolatile(common::cmd::OrderCommand* cmd,
                                       common::MatcherTradeEvent* eventHead) {
    if (eventHead == nullptr) {
        return;
    }

    common::MatcherTradeEvent* tail = eventHead->FindTail();

    // Use atomic compare-and-swap to append events
    // Initial load must also be atomic to avoid data race with concurrent CAS
    auto* atomicPtr =
        reinterpret_cast<std::atomic<common::MatcherTradeEvent*>*>(&cmd->matcherEvent);
    common::MatcherTradeEvent* expected = atomicPtr->load(std::memory_order_relaxed);
    do {
        tail->nextEvent = expected;
    } while (!atomicPtr->compare_exchange_weak(
        expected, eventHead, std::memory_order_release, std::memory_order_relaxed));
}

}  // namespace utils
}  // namespace core
}  // namespace exchange
