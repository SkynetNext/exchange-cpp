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

#include <cstdint>

namespace exchange::core::tests::nasdaq {

/**
 * NasdaqReader - utility class for reading NASDAQ ITCH50 data files
 * Note: Requires external library (paritytrading juncture) for ITCH50 parsing
 */
class NasdaqReader {
public:
  /**
   * Hash order ID to user ID
   * @param orderId - order ID
   * @param numUsersMask - mask for number of users (power of 2 - 1)
   * @return user ID (1-based)
   */
  static int32_t HashToUid(int64_t orderId, int32_t numUsersMask);

  /**
   * Convert time from high/low parts
   * @param high - high 32 bits
   * @param low - low 32 bits
   * @return combined time value
   */
  static int64_t ConvertTime(int32_t high, int64_t low);
};

}  // namespace exchange::core::tests::nasdaq
