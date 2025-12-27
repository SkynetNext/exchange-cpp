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

#include <exchange/core/IEventsHandler.h>
#include <exchange/core/SimpleEventsProcessor.h>
#include <exchange/core/common/CoreSymbolSpecification.h>
#include <exchange/core/common/OrderAction.h>
#include <exchange/core/common/OrderType.h>
#include <exchange/core/common/api/ApiPlaceOrder.h>
#include <exchange/core/common/cmd/CommandResultCode.h>
#include <exchange/core/common/config/PerformanceConfiguration.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace exchange {
namespace core {
namespace tests {
namespace integration {

/**
 * RejectionCause - enum for rejection test causes
 */
enum class RejectionCause {
  NO_REJECTION,
  REJECTION_BY_SIZE,
  REJECTION_BY_BUDGET
};

/**
 * ITExchangeCoreIntegrationRejection - abstract base class for rejection tests
 * Tests order rejection scenarios (NSF, size limits, budget limits)
 */
class ITExchangeCoreIntegrationRejection : public ::testing::Test {
public:
  ITExchangeCoreIntegrationRejection();
  virtual ~ITExchangeCoreIntegrationRejection() = default;

  /**
   * Get performance configuration (must be implemented by child class)
   */
  virtual exchange::core::common::config::PerformanceConfiguration
  GetPerformanceConfiguration() = 0;

protected:
  void SetUp() override;

  // Test helper methods
  void TestMultiBuy(const exchange::core::common::CoreSymbolSpecification
                        &symbolSpec,
                    exchange::core::common::OrderType orderType,
                    RejectionCause rejectionCause);

  void TestMultiSell(const exchange::core::common::CoreSymbolSpecification
                         &symbolSpec,
                     exchange::core::common::OrderType orderType,
                     RejectionCause rejectionCause);

  // Mock event handler
  class MockEventsHandler : public exchange::core::IEventsHandler {
  public:
    MOCK_METHOD(void, CommandResult,
                (const exchange::core::ApiCommandResult &), (override));
    MOCK_METHOD(void, TradeEvent, (const exchange::core::TradeEvent &),
                (override));
    MOCK_METHOD(void, RejectEvent, (const exchange::core::RejectEvent &),
                (override));
    MOCK_METHOD(void, ReduceEvent, (const exchange::core::ReduceEvent &),
                (override));
    MOCK_METHOD(void, OrderBook, (const exchange::core::OrderBook &),
                (override));
  };

  ::testing::StrictMock<MockEventsHandler> mockHandler_;
  std::unique_ptr<exchange::core::SimpleEventsProcessor> processor_;

private:
  exchange::core::common::api::ApiPlaceOrder
  BuilderPlace(int32_t symbolId, int64_t uid,
               exchange::core::common::OrderAction action,
               exchange::core::common::OrderType type);
};

} // namespace integration
} // namespace tests
} // namespace core
} // namespace exchange

