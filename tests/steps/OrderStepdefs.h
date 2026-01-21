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

#include <exchange/core/common/CoreSymbolSpecification.h>
#include <exchange/core/common/L2MarketData.h>
#include <exchange/core/common/MatcherEventType.h>
#include <exchange/core/common/MatcherTradeEvent.h>
#include <exchange/core/common/Order.h>
#include <exchange/core/common/OrderAction.h>
#include <exchange/core/common/OrderType.h>
#include <exchange/core/common/api/ApiAddUser.h>
#include <exchange/core/common/api/ApiAdjustUserBalance.h>
#include <exchange/core/common/api/ApiCancelOrder.h>
#include <exchange/core/common/api/ApiCommand.h>
#include <exchange/core/common/api/ApiMoveOrder.h>
#include <exchange/core/common/api/ApiPlaceOrder.h>
#include <exchange/core/common/api/reports/SingleUserReportResult.h>
#include <exchange/core/common/cmd/CommandResultCode.h>
#include <exchange/core/common/cmd/OrderCommand.h>
#include <exchange/core/common/cmd/OrderCommandType.h>
#include <exchange/core/common/config/PerformanceConfiguration.h>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace exchange {
namespace core {
namespace tests {
namespace util {
class ExchangeTestContainer;
class L2MarketDataHelper;
}  // namespace util
}  // namespace tests
}  // namespace core
}  // namespace exchange

namespace exchange {
namespace core {
namespace tests {
namespace steps {

/**
 * OrderStepdefs - Step definitions for order-related BDD tests
 * Matches Java version: exchange.core2.tests.steps.OrderStepdefs
 */
class OrderStepdefs {
public:
  static exchange::core::common::config::PerformanceConfiguration testPerformanceConfiguration;

  OrderStepdefs();
  ~OrderStepdefs();

  void Before();
  void After();

  // Test step methods (matching Java Cucumber steps)

  // Given: New client {user} has a balance:
  void NewClientHasBalance(int64_t clientId,
                           const std::vector<std::pair<std::string, int64_t>>& balanceEntries);

  // When: A client {user} places an {word} order {long} at {long}@{long}
  // (type: {word}, symbol: {symbol})
  void ClientPlacesOrder(int64_t clientId,
                         const std::string& side,
                         int64_t orderId,
                         int64_t price,
                         int64_t size,
                         const std::string& orderType,
                         const exchange::core::common::CoreSymbolSpecification& symbol);

  // When: A client {user} places an {word} order {long} at {long}@{long}
  // (type: {word}, symbol: {symbol}, reservePrice: {long})
  void
  ClientPlacesOrderWithReservePrice(int64_t clientId,
                                    const std::string& side,
                                    int64_t orderId,
                                    int64_t price,
                                    int64_t size,
                                    const std::string& orderType,
                                    const exchange::core::common::CoreSymbolSpecification& symbol,
                                    int64_t reservePrice);

  // When: A client {user} could not place an {word} order {long} at
  // {long}@{long} (type: {word}, symbol: {symbol}, reservePrice: {long}) due
  // to {word}
  void ClientCouldNotPlaceOrder(int64_t clientId,
                                const std::string& side,
                                int64_t orderId,
                                int64_t price,
                                int64_t size,
                                const std::string& orderType,
                                const exchange::core::common::CoreSymbolSpecification& symbol,
                                int64_t reservePrice,
                                const std::string& resultCode);

  // Then: The order {long} is partially matched. LastPx: {long}, LastQty:
  // {long}
  void OrderIsPartiallyMatched(int64_t orderId, int64_t lastPx, int64_t lastQty);

  // Then: The order {long} is fully matched. LastPx: {long}, LastQty: {long}
  void OrderIsFullyMatched(int64_t orderId, int64_t lastPx, int64_t lastQty);

  // Then: The order {long} is fully matched. LastPx: {long}, LastQty: {long},
  // bidderHoldPrice: {long}
  void OrderIsFullyMatchedWithBidderHoldPrice(int64_t orderId,
                                              int64_t lastPx,
                                              int64_t lastQty,
                                              int64_t bidderHoldPrice);

  // And: No trade events
  void NoTradeEvents();

  // When: A client {user} moves a price to {long} of the order {long}
  void ClientMovesOrderPrice(int64_t clientId, int64_t newPrice, int64_t orderId);

  // When: A client {user} could not move a price to {long} of the order {long}
  // due to {word}
  void ClientCouldNotMoveOrderPrice(int64_t clientId,
                                    int64_t newPrice,
                                    int64_t orderId,
                                    const std::string& resultCode);

  // Then: An {symbol} order book is:
  void OrderBookIs(const exchange::core::common::CoreSymbolSpecification& symbol,
                   const exchange::core::tests::util::L2MarketDataHelper& expectedOrderBook);

  // And: A balance of a client {user}:
  void ClientBalanceIs(int64_t clientId,
                       const std::vector<std::pair<std::string, int64_t>>& balanceEntries);

  // And: A client {user} orders:
  void ClientOrders(int64_t clientId,
                    const std::vector<std::map<std::string, std::string>>& orderEntries);

  // And: A client {user} does not have active orders
  void ClientHasNoActiveOrders(int64_t clientId);

  // Given: {long} {word} is added to the balance of a client {user}
  void AddBalanceToClient(int64_t amount, const std::string& currency, int64_t clientId);

  // When: A client {user} cancels the remaining size {long} of the order {long}
  void ClientCancelsOrder(int64_t clientId, int64_t size, int64_t orderId);

  // Helper methods
  exchange::core::tests::util::ExchangeTestContainer* GetContainer() const {
    return container_.get();
  }

private:
  void AClientPassAnOrder(int64_t clientId,
                          const std::string& side,
                          int64_t orderId,
                          int64_t price,
                          int64_t size,
                          const std::string& orderType,
                          const exchange::core::common::CoreSymbolSpecification& symbol,
                          int64_t reservePrice,
                          exchange::core::common::cmd::CommandResultCode expectedResultCode);

  void TheOrderIsMatched(int64_t orderId,
                         int64_t lastPx,
                         int64_t lastQty,
                         bool completed,
                         int64_t* bidderHoldPrice);

  void MoveOrder(int64_t clientId,
                 int64_t newPrice,
                 int64_t orderId,
                 exchange::core::common::cmd::CommandResultCode expectedResultCode);

  void CheckField(const std::map<std::string, int>& fieldNameByIndex,
                  const std::vector<std::string>& record,
                  const std::string& field,
                  int64_t expected);

  std::map<int64_t, exchange::core::common::Order>
  FetchIndexedOrders(const exchange::core::common::api::reports::SingleUserReportResult& profile);

  std::unique_ptr<exchange::core::tests::util::ExchangeTestContainer> container_;
  std::vector<exchange::core::common::MatcherTradeEvent*> matcherEvents_;
  std::map<int64_t, exchange::core::common::api::ApiPlaceOrder> orders_;

  std::map<std::string, exchange::core::common::CoreSymbolSpecification> symbolSpecificationMap_;
  std::map<std::string, int64_t> users_;
};

}  // namespace steps
}  // namespace tests
}  // namespace core
}  // namespace exchange
