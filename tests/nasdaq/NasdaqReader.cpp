/*
 * Copyright 2019 Maksim Zheravin
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

#include "NasdaqReader.h"

namespace exchange {
namespace core2 {
namespace tests {
namespace nasdaq {

int32_t NasdaqReader::HashToUid(int64_t orderId, int32_t numUsersMask) {
  int64_t x = ((orderId * 0xcc9e2d51LL) << 15) * 0x1b873593LL;
  return 1 + ((static_cast<int32_t>(x >> 32) ^ static_cast<int32_t>(x)) &
              numUsersMask);
}

int64_t NasdaqReader::ConvertTime(int32_t high, int64_t low) {
  return low + (static_cast<int64_t>(high) << 32);
}

} // namespace nasdaq
} // namespace tests
} // namespace core2
} // namespace exchange
