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

#pragma once

#include "StateHash.h"
#include "UserStatus.h"
#include "SymbolPositionRecord.h"
#include <ankerl/unordered_dense.h>
#include <cstdint>
#include <string>

namespace exchange {
namespace core {
namespace common {

class UserProfile : public StateHash {
public:
  int64_t uid = 0;

  // symbol -> margin position records
  // TODO initialize lazily (only needed if margin trading allowed)
  ankerl::unordered_dense::map<int32_t, SymbolPositionRecord *> positions;

  // protects from double adjustment
  int64_t adjustmentsCounter = 0;

  // currency accounts
  // currency -> balance
  ankerl::unordered_dense::map<int32_t, int64_t> accounts;

  UserStatus userStatus = UserStatus::ACTIVE;

  UserProfile() = default;

  UserProfile(int64_t uid, UserStatus userStatus);

  SymbolPositionRecord *GetPositionRecordOrThrowEx(int32_t symbol);

  // StateHash interface implementation
  int32_t GetStateHash() const override;

  std::string ToString() const;
};

} // namespace common
} // namespace core
} // namespace exchange

