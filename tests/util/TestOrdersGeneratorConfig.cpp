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

#include "TestOrdersGeneratorConfig.h"

namespace exchange {
namespace core {
namespace tests {
namespace util {

int TestOrdersGeneratorConfig::CalculateReadySeq() const {
  switch (preFillMode) {
    case PreFillMode::ORDERS_NUMBER:
      return targetOrderBookOrdersTotal;
    case PreFillMode::ORDERS_NUMBER_PLUS_QUARTER:
      return targetOrderBookOrdersTotal * 5 / 4;
    default:
      return targetOrderBookOrdersTotal;
  }
}

}  // namespace util
}  // namespace tests
}  // namespace core
}  // namespace exchange
