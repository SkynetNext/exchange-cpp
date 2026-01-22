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

#include <cstdint>
#include <set>
#include "TestOrdersGeneratorConfig.h"

namespace exchange::core::tests::util {

/**
 * AllowedSymbolTypes - types of symbols allowed in test
 */
enum class AllowedSymbolTypes { FUTURES_CONTRACT, CURRENCY_EXCHANGE_PAIR, BOTH };

/**
 * TestDataParameters - parameters for test data generation
 */
struct TestDataParameters {
  int totalTransactionsNumber;
  int targetOrderBookOrdersTotal;
  int numAccounts;
  std::set<int32_t> currenciesAllowed;
  int numSymbols;
  AllowedSymbolTypes allowedSymbolTypes;
  PreFillMode preFillMode;
  bool avalancheIOC;

  /**
   * Builder for single pair margin test
   */
  static TestDataParameters SinglePairMargin();

  /**
   * Builder for single pair exchange test
   */
  static TestDataParameters SinglePairExchange();

  /**
   * Builder for medium exchange test data configuration
   * - 1M active users (3M currency accounts)
   * - 1M pending limit-orders
   * - 10K symbols
   */
  static TestDataParameters Medium();

  /**
   * Builder for large exchange test data configuration
   * - 3M active users (10M currency accounts)
   * - 3M pending limit-orders
   * - 50K symbols
   */
  static TestDataParameters Large();

  /**
   * Builder for huge exchange test data configuration
   * - 10M active users (33M currency accounts)
   * - 30M pending limit-orders
   * - 100K symbols
   */
  static TestDataParameters Huge();
};

}  // namespace exchange::core::tests::util
