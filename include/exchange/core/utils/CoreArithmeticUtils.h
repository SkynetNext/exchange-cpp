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
#include "../common/CoreSymbolSpecification.h"

namespace exchange::core::utils {

/**
 * CoreArithmeticUtils - core arithmetic utilities for order calculations
 */
class CoreArithmeticUtils {
 public:
    /**
     * Calculate amount for ASK order
     */
    static int64_t CalculateAmountAsk(int64_t size, const common::CoreSymbolSpecification* spec);

    /**
     * Calculate amount for BID order
     */
    static int64_t CalculateAmountBid(int64_t size,
                                      int64_t price,
                                      const common::CoreSymbolSpecification* spec);

    /**
     * Calculate amount for BID order with taker fee
     */
    static int64_t CalculateAmountBidTakerFee(int64_t size,
                                              int64_t price,
                                              const common::CoreSymbolSpecification* spec);

    /**
     * Calculate amount for BID order release correction for maker
     */
    static int64_t CalculateAmountBidReleaseCorrMaker(int64_t size,
                                                      int64_t priceDiff,
                                                      const common::CoreSymbolSpecification* spec);

    /**
     * Calculate amount for BID order with taker fee for budget
     */
    static int64_t CalculateAmountBidTakerFeeForBudget(int64_t size,
                                                       int64_t budgetInSteps,
                                                       const common::CoreSymbolSpecification* spec);
};

}  // namespace exchange::core::utils
