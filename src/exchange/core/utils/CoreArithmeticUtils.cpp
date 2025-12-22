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

#include <exchange/core/utils/CoreArithmeticUtils.h>
#include <exchange/core/common/CoreSymbolSpecification.h>

namespace exchange {
namespace core {
namespace utils {

int64_t CoreArithmeticUtils::CalculateAmountAsk(
    int64_t size, const common::CoreSymbolSpecification *spec) {
  return size * spec->baseScaleK;
}

int64_t CoreArithmeticUtils::CalculateAmountBid(
    int64_t size, int64_t price, const common::CoreSymbolSpecification *spec) {
  return size * (price * spec->quoteScaleK);
}

int64_t CoreArithmeticUtils::CalculateAmountBidTakerFee(
    int64_t size, int64_t price, const common::CoreSymbolSpecification *spec) {
  return size * (price * spec->quoteScaleK + spec->takerFee);
}

int64_t CoreArithmeticUtils::CalculateAmountBidReleaseCorrMaker(
    int64_t size, int64_t priceDiff,
    const common::CoreSymbolSpecification *spec) {
  return size * (priceDiff * spec->quoteScaleK + (spec->takerFee - spec->makerFee));
}

int64_t CoreArithmeticUtils::CalculateAmountBidTakerFeeForBudget(
    int64_t size, int64_t budgetInSteps,
    const common::CoreSymbolSpecification *spec) {
  return budgetInSteps * spec->quoteScaleK + size * spec->takerFee;
}

} // namespace utils
} // namespace core
} // namespace exchange

