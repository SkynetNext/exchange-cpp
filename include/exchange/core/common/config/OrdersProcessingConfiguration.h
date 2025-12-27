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

namespace exchange {
namespace core {
namespace common {
namespace config {

/**
 * OrdersProcessingConfiguration - order processing configuration
 */
class OrdersProcessingConfiguration {
public:
  enum class RiskProcessingMode {
    // risk processing is on, every currency/asset account is checked
    // independently
    FULL_PER_CURRENCY,
    // risk processing is off, any orders accepted and placed
    NO_RISK_PROCESSING
  };

  enum class MarginTradingMode {
    MARGIN_TRADING_DISABLED,
    MARGIN_TRADING_ENABLED
  };

  RiskProcessingMode riskProcessingMode;
  MarginTradingMode marginTradingMode;

  OrdersProcessingConfiguration(RiskProcessingMode riskProcessingMode,
                                MarginTradingMode marginTradingMode)
      : riskProcessingMode(riskProcessingMode),
        marginTradingMode(marginTradingMode) {}

  static OrdersProcessingConfiguration Default() {
    return OrdersProcessingConfiguration(
        RiskProcessingMode::FULL_PER_CURRENCY,
        MarginTradingMode::MARGIN_TRADING_ENABLED);
  }
};

} // namespace config
} // namespace common
} // namespace core
} // namespace exchange
