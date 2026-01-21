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

#include "UserCurrencyAccountsGenerator.h"
#include <exchange/core/common/SymbolType.h>
#include <exchange/core/utils/Logger.h>
#include <algorithm>
#include <cmath>
#include <random>
#include <set>
#include "ExecutionTime.h"

namespace exchange {
namespace core {
namespace tests {
namespace util {

std::vector<std::vector<bool>>
UserCurrencyAccountsGenerator::GenerateUsers(int accountsToCreate,
                                             const std::set<int32_t>& currencies) {
  LOG_DEBUG("Generating users with {} accounts ({} currencies)...", accountsToCreate,
            currencies.size());
  ExecutionTime executionTime;

  std::vector<std::vector<bool>> result;
  result.push_back({});  // uid=0 no accounts

  std::mt19937 rng(1);
  std::vector<int32_t> currencyCodes(currencies.begin(), currencies.end());

  // Simplified Pareto distribution approximation
  // Using exponential distribution as approximation
  std::exponential_distribution<double> paretoDist(1.0 / 1.5);

  int totalAccountsQuota = accountsToCreate;
  while (totalAccountsQuota > 0) {
    // Sample from Pareto distribution (min 1, max currencyCodes.size())
    double sample = paretoDist(rng);
    int accountsToOpen =
      std::min(std::max(1, static_cast<int>(std::ceil(sample))),
               std::min(static_cast<int>(currencyCodes.size()), totalAccountsQuota));

    std::vector<bool> bitSet;
    std::set<int32_t> selectedCurrencies;

    // Randomly select currencies for this user
    while (static_cast<int>(selectedCurrencies.size()) < accountsToOpen) {
      int currencyIndex = rng() % currencyCodes.size();
      selectedCurrencies.insert(currencyCodes[currencyIndex]);
    }

    // Convert to BitSet representation (using max currency code as size)
    int maxCurrency = *currencies.rbegin();
    bitSet.resize(maxCurrency + 1, false);
    for (int32_t currency : selectedCurrencies) {
      bitSet[currency] = true;
    }

    totalAccountsQuota -= accountsToOpen;
    result.push_back(bitSet);
  }

  LOG_DEBUG("Generated {} users with {} accounts up to {} different currencies in {}",
            result.size(), accountsToCreate, currencies.size(), executionTime.GetTimeFormatted());
  return result;
}

std::vector<int32_t> UserCurrencyAccountsGenerator::CreateUserListForSymbol(
  const std::vector<std::vector<bool>>& users2currencies,
  const exchange::core::common::CoreSymbolSpecification& spec,
  int symbolMessagesExpected) {
  // We would prefer to choose from same number of users as number of messages
  // to be generated in tests at least 2 users are required, but not more than
  // half of all users provided
  int numUsersToSelect =
    std::min(static_cast<int>(users2currencies.size()), std::max(2, symbolMessagesExpected / 5));

  std::vector<int32_t> uids;
  std::mt19937 rng(spec.symbolId);
  int uid = 1 + (rng() % (users2currencies.size() - 1));
  int c = 0;

  while (static_cast<int>(uids.size()) < numUsersToSelect
         && c < static_cast<int>(users2currencies.size())) {
    if (uid < static_cast<int>(users2currencies.size())) {
      const auto& accounts = users2currencies[uid];

      // Check if user has required currencies
      bool hasQuoteCurrency =
        (spec.quoteCurrency < static_cast<int>(accounts.size()) && accounts[spec.quoteCurrency]);
      bool hasBaseCurrency =
        (spec.type == exchange::core::common::SymbolType::FUTURES_CONTRACT)
        || (spec.baseCurrency < static_cast<int>(accounts.size()) && accounts[spec.baseCurrency]);

      if (hasQuoteCurrency && hasBaseCurrency) {
        uids.push_back(uid);
      }
    }

    uid++;
    if (uid == static_cast<int>(users2currencies.size())) {
      uid = 1;
    }
    c++;
  }

  return uids;
}

}  // namespace util
}  // namespace tests
}  // namespace core
}  // namespace exchange
