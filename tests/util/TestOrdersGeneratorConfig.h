/*
 * Copyright 2020 Maksim Zheravin
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

#include <exchange/core/common/CoreSymbolSpecification.h>
#include <vector>

namespace exchange::core::tests::util {

/**
 * PreFillMode - mode for pre-filling order book
 */
enum class PreFillMode {
  ORDERS_NUMBER,              // Fill to targetOrderBookOrdersTotal
  ORDERS_NUMBER_PLUS_QUARTER  // Fill to targetOrderBookOrdersTotal * 5 / 4
};

/**
 * TestOrdersGeneratorConfig - configuration for test order generation
 */
struct TestOrdersGeneratorConfig {
  std::vector<exchange::core::common::CoreSymbolSpecification> coreSymbolSpecifications;
  int totalTransactionsNumber;
  std::vector<std::vector<bool>> usersAccounts;  // BitSet representation
  int targetOrderBookOrdersTotal;
  int seed;
  bool avalancheIOC;
  PreFillMode preFillMode;

  /**
   * Calculate ready sequence number based on preFillMode
   */
  int CalculateReadySeq() const;
};

}  // namespace exchange::core::tests::util
