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

#include <exchange/core/common/UserProfile.h>
#include <exchange/core/common/SymbolPositionRecord.h>
#include <functional>
#include <sstream>
#include <stdexcept>

namespace exchange {
namespace core {
namespace common {

UserProfile::UserProfile(int64_t uid, UserStatus userStatus)
    : uid(uid), adjustmentsCounter(0), userStatus(userStatus) {}

SymbolPositionRecord *UserProfile::GetPositionRecordOrThrowEx(int32_t symbol) {
  auto it = positions.find(symbol);
  if (it == positions.end()) {
    throw std::runtime_error("not found position for symbol " + std::to_string(symbol));
  }
  return it->second;
}

int32_t UserProfile::GetStateHash() const {
  // Hash positions
  std::size_t positionsHash = 0;
  for (const auto &pair : positions) {
    std::size_t h1 = std::hash<int32_t>{}(pair.first);
    std::size_t h2 = std::hash<SymbolPositionRecord *>{}(pair.second);
    positionsHash ^= (h1 << 1) ^ (h2 << 2);
  }

  // Hash accounts
  std::size_t accountsHash = 0;
  for (const auto &pair : accounts) {
    std::size_t h1 = std::hash<int32_t>{}(pair.first);
    std::size_t h2 = std::hash<int64_t>{}(pair.second);
    accountsHash ^= (h1 << 1) ^ (h2 << 2);
  }

  std::size_t h1 = std::hash<int64_t>{}(uid);
  std::size_t h2 = positionsHash;
  std::size_t h3 = std::hash<int64_t>{}(adjustmentsCounter);
  std::size_t h4 = accountsHash;
  std::size_t h5 = std::hash<uint8_t>{}(UserStatusToCode(userStatus));

  return static_cast<int32_t>(h1 ^ (h2 << 1) ^ (h3 << 2) ^ (h4 << 3) ^ (h5 << 4));
}

std::string UserProfile::ToString() const {
  std::ostringstream oss;
  oss << "UserProfile{uid=" << uid << ", positions=" << positions.size()
      << ", accounts=" << accounts.size() << ", adjustmentsCounter="
      << adjustmentsCounter << ", userStatus="
      << static_cast<int>(UserStatusToCode(userStatus)) << "}";
  return oss.str();
}

} // namespace common
} // namespace core
} // namespace exchange

