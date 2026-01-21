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

#include "../common/MatcherTradeEvent.h"
#include "../common/cmd/CommandResultCode.h"
#include "../common/cmd/OrderCommand.h"

namespace exchange::core::utils {

/**
 * UnsafeUtils - atomic operations utilities
 * Uses std::atomic for thread-safe operations
 */
class UnsafeUtils {
public:
  /**
   * Set result code atomically with CAS
   */
  static void SetResultVolatile(common::cmd::OrderCommand* cmd,
                                bool result,
                                common::cmd::CommandResultCode successCode,
                                common::cmd::CommandResultCode failureCode);

  /**
   * Append events atomically with CAS
   */
  static void AppendEventsVolatile(common::cmd::OrderCommand* cmd,
                                   common::MatcherTradeEvent* eventHead);
};

}  // namespace exchange::core::utils
