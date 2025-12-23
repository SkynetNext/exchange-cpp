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

#include <cstdint>
#include <exchange/core/common/CoreSymbolSpecification.h>
#include <set>
#include <vector>

namespace exchange {
namespace core2 {
namespace tests {
namespace util {

/**
 * UserCurrencyAccountsGenerator - generates user currency accounts for testing
 */
class UserCurrencyAccountsGenerator {
public:
  /**
   * Generates random users and different currencies they should have
   * In average each user will have account for 4 symbols (between 1 and
   * currencies.size())
   *
   * @param accountsToCreate - target number of accounts to create
   * @param currencies - set of allowed currency codes
   * @return vector of BitSets, where each BitSet represents currencies for a
   * user (index = uid)
   */
  static std::vector<std::vector<bool>>
  GenerateUsers(int accountsToCreate, const std::set<int32_t> &currencies);

  /**
   * Create user list for a specific symbol
   * Selects users that have required currency accounts for the symbol
   *
   * @param users2currencies - vector of currency BitSets per user
   * @param spec - symbol specification
   * @param symbolMessagesExpected - expected number of messages for this
   * symbol
   * @return array of user IDs that can trade this symbol
   */
  static std::vector<int32_t> CreateUserListForSymbol(
      const std::vector<std::vector<bool>> &users2currencies,
      const exchange::core::common::CoreSymbolSpecification &spec,
      int symbolMessagesExpected);
};

} // namespace util
} // namespace tests
} // namespace core2
} // namespace exchange
