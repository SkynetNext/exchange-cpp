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

#include "OrderStepdefs.h"
#include "../util/TestConstants.h"
#include <exchange/core/common/config/PerformanceConfiguration.h>

using namespace exchange::core2::tests::util;

namespace exchange {
namespace core2 {
namespace tests {
namespace steps {

exchange::core::common::config::PerformanceConfiguration
    OrderStepdefs::testPerformanceConfiguration =
        exchange::core::common::config::PerformanceConfiguration::Default();

OrderStepdefs::OrderStepdefs() {
  symbolSpecificationMap_["EUR_USD"] = TestConstants::SYMBOLSPEC_EUR_USD();
  symbolSpecificationMap_["ETH_XBT"] = TestConstants::SYMBOLSPEC_ETH_XBT();
  users_["Alice"] = 1440001L;
  users_["Bob"] = 1440002L;
  users_["Charlie"] = 1440003L;

  // Note: Cucumber step definitions would be registered here
  // when Cucumber C++ library is available
}

OrderStepdefs::~OrderStepdefs() = default;

void OrderStepdefs::Before() {
  // Note: Requires ExchangeTestContainer implementation
  // container_ = ExchangeTestContainer::Create(testPerformanceConfiguration);
  // container_->InitBasicSymbols();
}

void OrderStepdefs::After() { container_.reset(); }

} // namespace steps
} // namespace tests
} // namespace core2
} // namespace exchange
