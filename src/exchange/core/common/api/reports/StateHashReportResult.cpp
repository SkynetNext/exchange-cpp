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
#include <exchange/core/common/api/reports/StateHashReportResult.h>
#include <exchange/core/utils/SerializationUtils.h>

namespace exchange {
namespace core {
namespace common {
namespace api {
namespace reports {

StateHashReportResult::SubmoduleKey::SubmoduleKey(BytesIn &bytes) {
  moduleId = bytes.ReadInt();
  int32_t submoduleCode = bytes.ReadInt();
  // Convert code to SubmoduleType enum
  submodule = static_cast<SubmoduleType>(submoduleCode);
}

void StateHashReportResult::SubmoduleKey::WriteMarshallable(
    BytesOut &bytes) const {
  bytes.WriteInt(moduleId);
  bytes.WriteInt(static_cast<int32_t>(submodule));
}

void StateHashReportResult::WriteMarshallable(BytesOut &bytes) const {
  // Match Java: SerializationUtils.marshallGenericMap(hashCodes, bytes, (b, k)
  // -> k.writeMarshallable(b), BytesOut::writeInt) Directly implement to avoid
  // lambda type inference issues
  bytes.WriteInt(static_cast<int32_t>(hashCodes.size()));
  for (const auto &pair : hashCodes) {
    pair.first.WriteMarshallable(bytes);
    bytes.WriteInt(pair.second);
  }
}

} // namespace reports
} // namespace api
} // namespace common
} // namespace core
} // namespace exchange
