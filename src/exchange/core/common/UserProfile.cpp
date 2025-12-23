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
#include <exchange/core/common/SymbolPositionRecord.h>
#include <exchange/core/common/UserProfile.h>
#include <exchange/core/common/UserStatus.h>
#include <exchange/core/utils/HashingUtils.h>
#include <exchange/core/utils/SerializationUtils.h>
#include <functional>
#include <sstream>
#include <stdexcept>

namespace exchange {
namespace core {
namespace common {

UserProfile::UserProfile(int64_t uid, UserStatus userStatus)
    : uid(uid), adjustmentsCounter(0), userStatus(userStatus) {}

UserProfile::UserProfile(BytesIn *bytes) {
  if (bytes == nullptr) {
    throw std::invalid_argument("BytesIn cannot be nullptr");
  }
  uid = bytes->ReadLong();

  // positions (int -> SymbolPositionRecord*)
  int length = bytes->ReadInt();
  for (int i = 0; i < length; i++) {
    int32_t symbol = bytes->ReadInt();
    SymbolPositionRecord *record = new SymbolPositionRecord(uid, *bytes);
    positions[symbol] = record;
  }

  // adjustmentsCounter
  adjustmentsCounter = bytes->ReadLong();

  // account balances (int -> long)
  accounts = utils::SerializationUtils::ReadIntLongHashMap(*bytes);

  // userStatus
  userStatus = UserStatusFromCode(bytes->ReadByte());
}

SymbolPositionRecord *UserProfile::GetPositionRecordOrThrowEx(int32_t symbol) {
  auto it = positions.find(symbol);
  if (it == positions.end()) {
    throw std::runtime_error("not found position for symbol " +
                             std::to_string(symbol));
  }
  return it->second;
}

int32_t UserProfile::GetStateHash() const {
  // Match Java Objects.hash(uid, HashingUtils.stateHash(positions),
  // adjustmentsCounter, accounts.hashCode(), userStatus.hashCode())
  // Objects.hash uses: result = 31 * result + (element == null ? 0 :
  // element.hashCode())
  int32_t result = 1;

  // uid
  result = 31 * result + static_cast<int32_t>(uid ^ (uid >> 32));

  // HashingUtils.stateHash(positions)
  int32_t positionsHash = utils::HashingUtils::StateHash(positions);
  result = 31 * result + positionsHash;

  // adjustmentsCounter
  result = 31 * result + static_cast<int32_t>(adjustmentsCounter ^
                                              (adjustmentsCounter >> 32));

  // accounts.hashCode() - use HashingUtils for consistency
  // Note: Java's IntLongHashMap.hashCode() might be different, but we use
  // HashingUtils for consistency
  int32_t accountsHash = 0;
  for (const auto &pair : accounts) {
    std::size_t h1 = std::hash<int32_t>{}(pair.first);
    std::size_t h2 = std::hash<int64_t>{}(pair.second);
    accountsHash = 31 * accountsHash + static_cast<int32_t>(h1 ^ (h2 << 1));
  }
  result = 31 * result + accountsHash;

  // userStatus.hashCode()
  result = 31 * result + static_cast<int32_t>(UserStatusToCode(userStatus));

  return result;
}

std::string UserProfile::ToString() const {
  std::ostringstream oss;
  oss << "UserProfile{uid=" << uid << ", positions=" << positions.size()
      << ", accounts=" << accounts.size()
      << ", adjustmentsCounter=" << adjustmentsCounter
      << ", userStatus=" << static_cast<int>(UserStatusToCode(userStatus))
      << "}";
  return oss.str();
}

void UserProfile::WriteMarshallable(BytesOut &bytes) {
  bytes.WriteLong(uid);

  // positions (int -> SymbolPositionRecord*)
  utils::SerializationUtils::MarshallIntHashMap(positions, bytes);

  // adjustmentsCounter
  bytes.WriteLong(adjustmentsCounter);

  // account balances (int -> long)
  utils::SerializationUtils::MarshallIntLongHashMap(accounts, bytes);

  // userStatus
  bytes.WriteByte(static_cast<int8_t>(userStatus));
}

} // namespace common
} // namespace core
} // namespace exchange
