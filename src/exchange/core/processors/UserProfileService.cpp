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
#include <exchange/core/common/UserStatus.h>
#include <exchange/core/processors/UserProfileService.h>

namespace exchange {
namespace core {
namespace processors {

UserProfileService::UserProfileService() {}

common::UserProfile *UserProfileService::GetUserProfile(int64_t uid) {
  auto it = userProfiles.find(uid);
  return (it != userProfiles.end()) ? it->second : nullptr;
}

common::UserProfile *
UserProfileService::GetUserProfileOrAddSuspended(int64_t uid) {
  auto it = userProfiles.find(uid);
  if (it != userProfiles.end()) {
    return it->second;
  }
  // Create new suspended user profile
  auto *profile = new common::UserProfile(uid, common::UserStatus::SUSPENDED);
  userProfiles[uid] = profile;
  return profile;
}

common::cmd::CommandResultCode
UserProfileService::BalanceAdjustment(int64_t uid, int32_t currency,
                                      int64_t amount,
                                      int64_t fundingTransactionId) {
  common::UserProfile *profile = GetUserProfile(uid);
  if (profile == nullptr) {
    return common::cmd::CommandResultCode::USER_MGMT_USER_NOT_FOUND;
  }

  // Double settlement protection
  if (profile->adjustmentsCounter == fundingTransactionId) {
    return common::cmd::CommandResultCode::
        USER_MGMT_ACCOUNT_BALANCE_ADJUSTMENT_ALREADY_APPLIED_SAME;
  }
  if (profile->adjustmentsCounter > fundingTransactionId) {
    return common::cmd::CommandResultCode::
        USER_MGMT_ACCOUNT_BALANCE_ADJUSTMENT_ALREADY_APPLIED_MANY;
  }

  // Validate balance for withdrawals
  auto accountIt = profile->accounts.find(currency);
  int64_t currentBalance =
      (accountIt != profile->accounts.end()) ? accountIt->second : 0;
  if (amount < 0 && (currentBalance + amount < 0)) {
    return common::cmd::CommandResultCode::
        USER_MGMT_ACCOUNT_BALANCE_ADJUSTMENT_NSF;
  }

  // Update adjustment counter and balance
  profile->adjustmentsCounter = fundingTransactionId;
  profile->accounts[currency] = currentBalance + amount;

  return common::cmd::CommandResultCode::SUCCESS;
}

common::cmd::CommandResultCode UserProfileService::AddUser(int64_t uid) {
  if (GetUserProfile(uid) != nullptr) {
    return common::cmd::CommandResultCode::USER_MGMT_USER_ALREADY_EXISTS;
  }

  auto *profile = new common::UserProfile(uid, common::UserStatus::ACTIVE);
  userProfiles[uid] = profile;
  return common::cmd::CommandResultCode::SUCCESS;
}

common::cmd::CommandResultCode UserProfileService::SuspendUser(int64_t uid) {
  common::UserProfile *profile = GetUserProfile(uid);
  if (profile == nullptr) {
    return common::cmd::CommandResultCode::USER_MGMT_USER_NOT_FOUND;
  }

  if (profile->userStatus == common::UserStatus::SUSPENDED) {
    return common::cmd::CommandResultCode::USER_MGMT_USER_ALREADY_SUSPENDED;
  }

  // Check if user has open positions
  for (const auto &pos : profile->positions) {
    if (pos.second != nullptr && !pos.second->IsEmpty()) {
      return common::cmd::CommandResultCode::
          USER_MGMT_USER_NOT_SUSPENDABLE_HAS_POSITIONS;
    }
  }

  // Check if user has non-zero accounts
  for (const auto &acc : profile->accounts) {
    if (acc.second != 0) {
      return common::cmd::CommandResultCode::
          USER_MGMT_USER_NOT_SUSPENDABLE_NON_EMPTY_ACCOUNTS;
    }
  }

  profile->userStatus = common::UserStatus::SUSPENDED;
  return common::cmd::CommandResultCode::SUCCESS;
}

common::cmd::CommandResultCode UserProfileService::ResumeUser(int64_t uid) {
  common::UserProfile *profile = GetUserProfile(uid);
  if (profile == nullptr) {
    return common::cmd::CommandResultCode::USER_MGMT_USER_NOT_FOUND;
  }

  profile->userStatus = common::UserStatus::ACTIVE;
  return common::cmd::CommandResultCode::SUCCESS;
}

std::vector<common::UserProfile *> UserProfileService::GetUserProfiles() const {
  std::vector<common::UserProfile *> result;
  result.reserve(userProfiles.size());
  for (const auto &pair : userProfiles) {
    result.push_back(pair.second);
  }
  return result;
}

void UserProfileService::Reset() { userProfiles.clear(); }

int32_t UserProfileService::GetStateHash() const {
  std::size_t hash = 0;
  for (const auto &pair : userProfiles) {
    std::size_t h1 = std::hash<int64_t>{}(pair.first);
    if (pair.second != nullptr) {
      h1 ^= static_cast<std::size_t>(pair.second->GetStateHash());
    }
    hash ^= (h1 << 1);
  }
  return static_cast<int32_t>(hash);
}

} // namespace processors
} // namespace core
} // namespace exchange
