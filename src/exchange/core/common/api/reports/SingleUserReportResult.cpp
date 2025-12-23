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

#include <exchange/core/common/BytesIn.h>
#include <exchange/core/common/BytesOut.h>
#include <exchange/core/common/Order.h>
#include <exchange/core/common/PositionDirection.h>
#include <exchange/core/common/UserStatus.h>
#include <exchange/core/common/api/reports/SingleUserReportResult.h>

namespace exchange {
namespace core {
namespace common {
namespace api {
namespace reports {

SingleUserReportResult::Position::Position(BytesIn &bytes) {
  quoteCurrency = bytes.ReadInt();
  direction = PositionDirectionFromCode(bytes.ReadByte());
  openVolume = bytes.ReadLong();
  openPriceSum = bytes.ReadLong();
  profit = bytes.ReadLong();
  pendingSellSize = bytes.ReadLong();
  pendingBuySize = bytes.ReadLong();
}

void SingleUserReportResult::Position::WriteMarshallable(
    BytesOut &bytes) const {
  bytes.WriteInt(quoteCurrency);
  bytes.WriteByte(static_cast<int8_t>(GetMultiplier(direction)));
  bytes.WriteLong(openVolume);
  bytes.WriteLong(openPriceSum);
  bytes.WriteLong(profit);
  bytes.WriteLong(pendingSellSize);
  bytes.WriteLong(pendingBuySize);
}

} // namespace reports
} // namespace api
} // namespace common
} // namespace core
} // namespace exchange
