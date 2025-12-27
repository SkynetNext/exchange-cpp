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
#include "../util/ExchangeTestContainer.h"
#include "../util/L2MarketDataHelper.h"
#include "../util/TestConstants.h"
#include <cassert>
#include <exchange/core/common/cmd/OrderCommandType.h>
#include <exchange/core/common/config/PerformanceConfiguration.h>
#include <stdexcept>

using namespace exchange::core::tests::util;

namespace exchange {
namespace core {
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
}

OrderStepdefs::~OrderStepdefs() = default;

void OrderStepdefs::Before() {
  container_ = ExchangeTestContainer::Create(testPerformanceConfiguration);
  container_->InitBasicSymbols();
  matcherEvents_.clear();
  orders_.clear();
}

void OrderStepdefs::After() {
  if (container_) {
    container_.reset();
  }
  matcherEvents_.clear();
  orders_.clear();
}

void OrderStepdefs::NewClientHasBalance(
    int64_t clientId,
    const std::vector<std::pair<std::string, int64_t>> &balanceEntries) {
  std::vector<exchange::core::common::api::ApiCommand *> cmds;

  // Add user
  cmds.push_back(new exchange::core::common::api::ApiAddUser(clientId));

  // Add balance adjustments
  int transactionId = 0;
  for (const auto &entry : balanceEntries) {
    transactionId++;
    int32_t currency = TestConstants::GetCurrency(entry.first);
    cmds.push_back(new exchange::core::common::api::ApiAdjustUserBalance(
        clientId, currency, entry.second, transactionId));
  }

  // Submit commands synchronously
  // Note: SubmitCommandsSync takes ownership of the commands (they are deleted
  // by the API)
  container_->GetApi()->SubmitCommandsSync(cmds);
}

void OrderStepdefs::ClientPlacesOrder(
    int64_t clientId, const std::string &side, int64_t orderId, int64_t price,
    int64_t size, const std::string &orderType,
    const exchange::core::common::CoreSymbolSpecification &symbol) {
  AClientPassAnOrder(clientId, side, orderId, price, size, orderType, symbol, 0,
                     exchange::core::common::cmd::CommandResultCode::SUCCESS);
}

void OrderStepdefs::ClientPlacesOrderWithReservePrice(
    int64_t clientId, const std::string &side, int64_t orderId, int64_t price,
    int64_t size, const std::string &orderType,
    const exchange::core::common::CoreSymbolSpecification &symbol,
    int64_t reservePrice) {
  AClientPassAnOrder(clientId, side, orderId, price, size, orderType, symbol,
                     reservePrice,
                     exchange::core::common::cmd::CommandResultCode::SUCCESS);
}

void OrderStepdefs::ClientCouldNotPlaceOrder(
    int64_t clientId, const std::string &side, int64_t orderId, int64_t price,
    int64_t size, const std::string &orderType,
    const exchange::core::common::CoreSymbolSpecification &symbol,
    int64_t reservePrice, const std::string &resultCodeStr) {
  // Convert string to CommandResultCode
  exchange::core::common::cmd::CommandResultCode resultCode;
  if (resultCodeStr == "SUCCESS") {
    resultCode = exchange::core::common::cmd::CommandResultCode::SUCCESS;
  } else if (resultCodeStr == "RISK_NSF") {
    resultCode = exchange::core::common::cmd::CommandResultCode::RISK_NSF;
  } else if (resultCodeStr == "RISK_INVALID_RESERVE_BID_PRICE") {
    resultCode = exchange::core::common::cmd::CommandResultCode::
        RISK_INVALID_RESERVE_BID_PRICE;
  } else {
    throw std::invalid_argument("Unknown result code: " + resultCodeStr);
  }

  AClientPassAnOrder(clientId, side, orderId, price, size, orderType, symbol,
                     reservePrice, resultCode);
}

void OrderStepdefs::OrderIsPartiallyMatched(int64_t orderId, int64_t lastPx,
                                            int64_t lastQty) {
  TheOrderIsMatched(orderId, lastPx, lastQty, false, nullptr);
}

void OrderStepdefs::OrderIsFullyMatched(int64_t orderId, int64_t lastPx,
                                        int64_t lastQty) {
  TheOrderIsMatched(orderId, lastPx, lastQty, true, nullptr);
}

void OrderStepdefs::OrderIsFullyMatchedWithBidderHoldPrice(
    int64_t orderId, int64_t lastPx, int64_t lastQty, int64_t bidderHoldPrice) {
  TheOrderIsMatched(orderId, lastPx, lastQty, true, &bidderHoldPrice);
}

void OrderStepdefs::NoTradeEvents() {
  if (matcherEvents_.size() != 0) {
    throw std::runtime_error("Expected no trade events, but got " +
                             std::to_string(matcherEvents_.size()));
  }
}

void OrderStepdefs::ClientMovesOrderPrice(int64_t clientId, int64_t newPrice,
                                          int64_t orderId) {
  MoveOrder(clientId, newPrice, orderId,
            exchange::core::common::cmd::CommandResultCode::SUCCESS);
}

void OrderStepdefs::ClientCouldNotMoveOrderPrice(
    int64_t clientId, int64_t newPrice, int64_t orderId,
    const std::string &resultCodeStr) {
  exchange::core::common::cmd::CommandResultCode resultCode;
  if (resultCodeStr == "SUCCESS") {
    resultCode = exchange::core::common::cmd::CommandResultCode::SUCCESS;
  } else if (resultCodeStr == "MATCHING_MOVE_FAILED_PRICE_OVER_RISK_LIMIT") {
    resultCode = exchange::core::common::cmd::CommandResultCode::
        MATCHING_MOVE_FAILED_PRICE_OVER_RISK_LIMIT;
  } else {
    throw std::invalid_argument("Unknown result code: " + resultCodeStr);
  }

  MoveOrder(clientId, newPrice, orderId, resultCode);
}

void OrderStepdefs::OrderBookIs(
    const exchange::core::common::CoreSymbolSpecification &symbol,
    const exchange::core::tests::util::L2MarketDataHelper &expectedOrderBook) {
  auto actualOrderBook = container_->RequestCurrentOrderBook(symbol.symbolId);
  auto expected = expectedOrderBook.Build();

  if (!actualOrderBook || !expected) {
    throw std::runtime_error("Order book is null");
  }

  if (*actualOrderBook != *expected) {
    throw std::runtime_error("Order book mismatch");
  }
}

void OrderStepdefs::ClientBalanceIs(
    int64_t clientId,
    const std::vector<std::pair<std::string, int64_t>> &balanceEntries) {
  auto profile = container_->GetUserProfile(clientId);
  if (!profile) {
    throw std::runtime_error("Failed to get user profile");
  }

  if (!profile->accounts) {
    throw std::runtime_error("User has no accounts");
  }

  for (const auto &entry : balanceEntries) {
    int32_t currency = TestConstants::GetCurrency(entry.first);
    auto it = profile->accounts->find(currency);
    int64_t actualBalance = 0;
    if (it != profile->accounts->end()) {
      actualBalance = it->second;
    }
    // If currency not found, treat as balance 0 (matches Java behavior where
    // getAccounts().get(currency) returns null for missing currencies)
    if (actualBalance != entry.second) {
      throw std::runtime_error("Unexpected balance of: " + entry.first +
                               ", expected: " + std::to_string(entry.second) +
                               ", got: " + std::to_string(actualBalance));
    }
  }
}

void OrderStepdefs::ClientOrders(
    int64_t clientId,
    const std::vector<std::map<std::string, std::string>> &orderEntries) {
  auto profile = container_->GetUserProfile(clientId);
  if (!profile) {
    throw std::runtime_error("Failed to get user profile");
  }

  auto orders = FetchIndexedOrders(*profile);

  for (const auto &orderEntry : orderEntries) {
    auto orderIdIt = orderEntry.find("id");
    if (orderIdIt == orderEntry.end()) {
      throw std::runtime_error("Order entry missing 'id' field");
    }

    int64_t orderId = std::stoll(orderIdIt->second);
    auto orderIt = orders.find(orderId);
    if (orderIt == orders.end()) {
      throw std::runtime_error("Order not found: " + std::to_string(orderId));
    }

    const auto &order = orderIt->second;

    // Check price
    auto priceIt = orderEntry.find("price");
    if (priceIt != orderEntry.end()) {
      int64_t expectedPrice = std::stoll(priceIt->second);
      if (order.price != expectedPrice) {
        throw std::runtime_error("Unexpected value for price: expected " +
                                 std::to_string(expectedPrice) + ", got " +
                                 std::to_string(order.price));
      }
    }

    // Check size
    auto sizeIt = orderEntry.find("size");
    if (sizeIt != orderEntry.end()) {
      int64_t expectedSize = std::stoll(sizeIt->second);
      if (order.size != expectedSize) {
        throw std::runtime_error("Unexpected value for size: expected " +
                                 std::to_string(expectedSize) + ", got " +
                                 std::to_string(order.size));
      }
    }

    // Check filled
    auto filledIt = orderEntry.find("filled");
    if (filledIt != orderEntry.end()) {
      int64_t expectedFilled = std::stoll(filledIt->second);
      if (order.filled != expectedFilled) {
        throw std::runtime_error("Unexpected value for filled: expected " +
                                 std::to_string(expectedFilled) + ", got " +
                                 std::to_string(order.filled));
      }
    }

    // Check reservePrice
    auto reservePriceIt = orderEntry.find("reservePrice");
    if (reservePriceIt != orderEntry.end()) {
      int64_t expectedReservePrice = std::stoll(reservePriceIt->second);
      if (order.reserveBidPrice != expectedReservePrice) {
        throw std::runtime_error(
            "Unexpected value for reservePrice: expected " +
            std::to_string(expectedReservePrice) + ", got " +
            std::to_string(order.reserveBidPrice));
      }
    }

    // Check side
    auto sideIt = orderEntry.find("side");
    if (sideIt != orderEntry.end()) {
      exchange::core::common::OrderAction expectedAction;
      if (sideIt->second == "ASK") {
        expectedAction = exchange::core::common::OrderAction::ASK;
      } else if (sideIt->second == "BID") {
        expectedAction = exchange::core::common::OrderAction::BID;
      } else {
        throw std::invalid_argument("Unknown side: " + sideIt->second);
      }

      if (order.action != expectedAction) {
        throw std::runtime_error("Unexpected action: expected " +
                                 sideIt->second);
      }
    }
  }
}

void OrderStepdefs::ClientHasNoActiveOrders(int64_t clientId) {
  auto profile = container_->GetUserProfile(clientId);
  if (!profile) {
    throw std::runtime_error("Failed to get user profile");
  }

  auto orders = FetchIndexedOrders(*profile);
  if (orders.size() != 0) {
    throw std::runtime_error("Expected no active orders, but got " +
                             std::to_string(orders.size()));
  }
}

void OrderStepdefs::AddBalanceToClient(int64_t amount,
                                       const std::string &currency,
                                       int64_t clientId) {
  int32_t currencyCode = TestConstants::GetCurrency(currency);
  auto adjustCmd =
      std::make_unique<exchange::core::common::api::ApiAdjustUserBalance>(
          clientId, currencyCode, amount, 2193842938742L);

  container_->SubmitCommandSync(
      std::move(adjustCmd),
      exchange::core::common::cmd::CommandResultCode::SUCCESS);
}

void OrderStepdefs::ClientCancelsOrder(int64_t clientId, int64_t size,
                                       int64_t orderId) {
  auto initialOrderIt = orders_.find(orderId);
  if (initialOrderIt == orders_.end()) {
    throw std::runtime_error("Order not found: " + std::to_string(orderId));
  }

  const auto &initialOrder = initialOrderIt->second;
  auto cancelCmd =
      std::make_unique<exchange::core::common::api::ApiCancelOrder>(
          orderId, clientId, initialOrder.symbol);

  container_->SubmitCommandSync(
      std::move(cancelCmd),
      [this, clientId, orderId, size,
       &initialOrder](const exchange::core::common::cmd::OrderCommand &cmd) {
        if (cmd.resultCode !=
            exchange::core::common::cmd::CommandResultCode::SUCCESS) {
          throw std::runtime_error("Cancel order failed");
        }
        if (cmd.command !=
            exchange::core::common::cmd::OrderCommandType::CANCEL_ORDER) {
          throw std::runtime_error("Unexpected command type");
        }
        if (cmd.orderId != orderId) {
          throw std::runtime_error("Unexpected orderId");
        }
        if (cmd.uid != clientId) {
          throw std::runtime_error("Unexpected uid");
        }
        if (cmd.symbol != initialOrder.symbol) {
          throw std::runtime_error("Unexpected symbol");
        }
        if (cmd.action != initialOrder.action) {
          throw std::runtime_error("Unexpected action");
        }

        if (!cmd.matcherEvent) {
          throw std::runtime_error("MatcherEvent is null");
        }

        const auto &evt = *cmd.matcherEvent;
        if (evt.eventType != exchange::core::common::MatcherEventType::REDUCE) {
          throw std::runtime_error("Unexpected event type");
        }
        if (evt.size != size) {
          throw std::runtime_error("Unexpected size: expected " +
                                   std::to_string(size) + ", got " +
                                   std::to_string(evt.size));
        }
      });
}

void OrderStepdefs::AClientPassAnOrder(
    int64_t clientId, const std::string &side, int64_t orderId, int64_t price,
    int64_t size, const std::string &orderType,
    const exchange::core::common::CoreSymbolSpecification &symbol,
    int64_t reservePrice,
    exchange::core::common::cmd::CommandResultCode expectedResultCode) {

  exchange::core::common::OrderAction action;
  if (side == "ASK") {
    action = exchange::core::common::OrderAction::ASK;
  } else if (side == "BID") {
    action = exchange::core::common::OrderAction::BID;
  } else {
    throw std::invalid_argument("Unknown side: " + side);
  }

  exchange::core::common::OrderType type;
  if (orderType == "GTC") {
    type = exchange::core::common::OrderType::GTC;
  } else if (orderType == "IOC") {
    type = exchange::core::common::OrderType::IOC;
  } else if (orderType == "IOC_BUDGET") {
    type = exchange::core::common::OrderType::IOC_BUDGET;
  } else if (orderType == "FOK") {
    type = exchange::core::common::OrderType::FOK;
  } else if (orderType == "FOK_BUDGET") {
    type = exchange::core::common::OrderType::FOK_BUDGET;
  } else {
    throw std::invalid_argument("Unknown order type: " + orderType);
  }

  auto order = std::make_unique<exchange::core::common::api::ApiPlaceOrder>(
      price, size, orderId, action, type, clientId, symbol.symbolId, 0,
      reservePrice);

  // Store order copy before releasing ownership
  // Use insert/emplace instead of operator[] since ApiPlaceOrder has no default
  // constructor
  orders_.emplace(orderId, *order);

  auto future =
      container_->GetApi()->SubmitCommandAsyncFullResponse(order.release());

  auto cmd = future.get();

  if (cmd.orderId != orderId) {
    throw std::runtime_error("Unexpected orderId");
  }
  if (cmd.resultCode != expectedResultCode) {
    throw std::runtime_error(
        "Unexpected resultCode: expected " +
        std::to_string(static_cast<int>(expectedResultCode)) + ", got " +
        std::to_string(static_cast<int>(cmd.resultCode)));
  }
  if (cmd.uid != clientId) {
    throw std::runtime_error("Unexpected uid");
  }
  if (cmd.price != price) {
    throw std::runtime_error("Unexpected price");
  }
  if (cmd.size != size) {
    throw std::runtime_error("Unexpected size");
  }
  if (cmd.action != action) {
    throw std::runtime_error("Unexpected action");
  }
  if (cmd.orderType != type) {
    throw std::runtime_error("Unexpected orderType");
  }
  if (cmd.symbol != symbol.symbolId) {
    throw std::runtime_error("Unexpected symbol");
  }
  if (cmd.reserveBidPrice != reservePrice) {
    throw std::runtime_error("Unexpected reserveBidPrice: expected " +
                             std::to_string(reservePrice) + ", got " +
                             std::to_string(cmd.reserveBidPrice));
  }

  matcherEvents_ = cmd.ExtractEvents();
}

void OrderStepdefs::TheOrderIsMatched(int64_t orderId, int64_t lastPx,
                                      int64_t lastQty, bool completed,
                                      int64_t *bidderHoldPrice) {
  if (matcherEvents_.size() != 1) {
    throw std::runtime_error("Expected 1 matcher event, got " +
                             std::to_string(matcherEvents_.size()));
  }

  const auto &evt = *matcherEvents_[0];
  if (evt.matchedOrderId != orderId) {
    throw std::runtime_error("Unexpected matchedOrderId");
  }

  auto orderIt = orders_.find(orderId);
  if (orderIt == orders_.end()) {
    throw std::runtime_error("Order not found: " + std::to_string(orderId));
  }

  if (evt.matchedOrderUid != orderIt->second.uid) {
    throw std::runtime_error("Unexpected matchedOrderUid");
  }
  if (evt.matchedOrderCompleted != completed) {
    throw std::runtime_error("Unexpected matchedOrderCompleted");
  }
  if (evt.eventType != exchange::core::common::MatcherEventType::TRADE) {
    throw std::runtime_error("Unexpected eventType");
  }
  if (evt.size != lastQty) {
    throw std::runtime_error("Unexpected size");
  }
  if (evt.price != lastPx) {
    throw std::runtime_error("Unexpected price");
  }
  if (bidderHoldPrice != nullptr && evt.bidderHoldPrice != *bidderHoldPrice) {
    throw std::runtime_error("Unexpected bidderHoldPrice");
  }
}

void OrderStepdefs::MoveOrder(
    int64_t clientId, int64_t newPrice, int64_t orderId,
    exchange::core::common::cmd::CommandResultCode expectedResultCode) {
  auto initialOrderIt = orders_.find(orderId);
  if (initialOrderIt == orders_.end()) {
    throw std::runtime_error("Order not found: " + std::to_string(orderId));
  }

  const auto &initialOrder = initialOrderIt->second;
  auto moveCmd = std::make_unique<exchange::core::common::api::ApiMoveOrder>(
      orderId, newPrice, clientId, initialOrder.symbol);

  container_->SubmitCommandSync(
      std::move(moveCmd),
      [this, expectedResultCode, clientId,
       orderId](const exchange::core::common::cmd::OrderCommand &cmd) {
        if (cmd.resultCode != expectedResultCode) {
          throw std::runtime_error("Unexpected resultCode");
        }
        if (cmd.orderId != orderId) {
          throw std::runtime_error("Unexpected orderId");
        }
        if (cmd.uid != clientId) {
          throw std::runtime_error("Unexpected uid");
        }

        matcherEvents_ = cmd.ExtractEvents();
      });
}

std::map<int64_t, exchange::core::common::Order>
OrderStepdefs::FetchIndexedOrders(
    const exchange::core::common::api::reports::SingleUserReportResult
        &profile) {
  std::map<int64_t, exchange::core::common::Order> result;

  if (!profile.orders) {
    return result;
  }

  for (const auto &[symbolId, orderList] : *profile.orders) {
    for (const auto *order : orderList) {
      if (order) {
        result[order->orderId] = *order;
      }
    }
  }

  return result;
}

} // namespace steps
} // namespace tests
} // namespace core
} // namespace exchange
