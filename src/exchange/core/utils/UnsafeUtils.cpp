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

namespace exchange::core::utils {

void UnsafeUtils::SetResultVolatile(common::cmd::OrderCommand* cmd,
                                    bool result,
                                    common::cmd::CommandResultCode successCode,
                                    common::cmd::CommandResultCode failureCode) {
    common::cmd::CommandResultCode codeToSet = result ? successCode : failureCode;

    // Use atomic_ref (C++20) for safer atomic operations on non-atomic fields
    // This avoids reinterpret_cast and is guaranteed to work correctly
    std::atomic_ref<common::cmd::CommandResultCode> atomicRef(cmd->resultCode);
    common::cmd::CommandResultCode expected = atomicRef.load(std::memory_order_relaxed);
    while (expected != codeToSet && expected != failureCode) {
        if (atomicRef.compare_exchange_weak(
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

    // Use atomic_ref (C++20) for safer atomic operations on non-atomic fields
    // This avoids reinterpret_cast and is guaranteed to work correctly
    std::atomic_ref<common::MatcherTradeEvent*> atomicRef(cmd->matcherEvent);
    common::MatcherTradeEvent* expected = atomicRef.load(std::memory_order_relaxed);
    do {
        tail->nextEvent = expected;
    } while (!atomicRef.compare_exchange_weak(
        expected, eventHead, std::memory_order_release, std::memory_order_relaxed));
}

}  // namespace exchange::core::utils
