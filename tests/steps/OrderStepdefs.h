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

#include <exchange/core/common/CoreSymbolSpecification.h>
#include <exchange/core/common/api/ApiPlaceOrder.h>
#include <exchange/core/common/config/PerformanceConfiguration.h>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace exchange {
namespace core2 {
namespace tests {
namespace util {
class ExchangeTestContainer;
class L2MarketDataHelper;
} // namespace util
} // namespace tests
} // namespace core2
} // namespace exchange

namespace exchange {
namespace core2 {
namespace tests {
namespace steps {

/**
 * OrderStepdefs - Cucumber step definitions for order-related BDD tests
 * Note: Requires Cucumber C++ library and ExchangeTestContainer implementation
 */
class OrderStepdefs {
public:
  static exchange::core::common::config::PerformanceConfiguration
      testPerformanceConfiguration;

  OrderStepdefs();
  ~OrderStepdefs();

  void Before();
  void After();

  // Note: Cucumber step definitions would be implemented here
  // when Cucumber C++ library is available

private:
  std::unique_ptr<exchange::core2::tests::util::ExchangeTestContainer>
      container_;
  std::vector<exchange::core::common::MatcherTradeEvent> matcherEvents_;
  std::map<int64_t, exchange::core::common::api::ApiPlaceOrder> orders_;

  std::map<std::string, exchange::core::common::CoreSymbolSpecification>
      symbolSpecificationMap_;
  std::map<std::string, int64_t> users_;
};

} // namespace steps
} // namespace tests
} // namespace core2
} // namespace exchange
