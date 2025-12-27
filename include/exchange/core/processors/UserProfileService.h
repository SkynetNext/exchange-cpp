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

#include "../common/StateHash.h"
#include "../common/UserProfile.h"
#include "../common/WriteBytesMarshallable.h"
#include "../common/cmd/CommandResultCode.h"
#include <ankerl/unordered_dense.h>
#include <cstdint>
#include <vector>

namespace exchange {
namespace core {
namespace common {
class BytesIn;
} // namespace common
namespace processors {

/**
 * UserProfileService - stateful user profile service
 * Manages user profiles (uid -> UserProfile)
 */
class UserProfileService : public common::StateHash,
                           public common::WriteBytesMarshallable {
public:
  // uid -> UserProfile
  ankerl::unordered_dense::map<int64_t, common::UserProfile *> userProfiles;

  UserProfileService();

  /**
   * Constructor from BytesIn (deserialization)
   */
  UserProfileService(common::BytesIn *bytes);

  /**
   * Find user profile
   * @param uid user ID
   * @return user profile or nullptr if not found
   */
  common::UserProfile *GetUserProfile(int64_t uid);

  /**
   * Get user profile or add suspended user if not exists
   */
  common::UserProfile *GetUserProfileOrAddSuspended(int64_t uid);

  /**
   * Perform balance adjustment for specific user
   */
  common::cmd::CommandResultCode
  BalanceAdjustment(int64_t uid, int32_t currency, int64_t amount,
                    int64_t fundingTransactionId);

  /**
   * Add new user
   */
  common::cmd::CommandResultCode AddUser(int64_t uid);

  /**
   * Suspend user
   */
  common::cmd::CommandResultCode SuspendUser(int64_t uid);

  /**
   * Resume user
   */
  common::cmd::CommandResultCode ResumeUser(int64_t uid);

  /**
   * Get all user profiles (for iteration)
   */
  std::vector<common::UserProfile *> GetUserProfiles() const;

  /**
   * Reset - clear all user profiles
   */
  void Reset();

  // StateHash interface
  int32_t GetStateHash() const override;

  // WriteBytesMarshallable interface
  void WriteMarshallable(common::BytesOut &bytes) const override;
};

} // namespace processors
} // namespace core
} // namespace exchange
