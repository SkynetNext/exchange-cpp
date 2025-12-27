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

#include <exchange/core/IEventsHandler.h>
#include <exchange/core/SimpleEventsProcessor.h>
#include <exchange/core/common/L2MarketData.h>
#include <exchange/core/common/MatcherEventType.h>
#include <exchange/core/common/MatcherTradeEvent.h>
#include <exchange/core/common/OrderAction.h>
#include <exchange/core/common/api/ApiAddUser.h>
#include <exchange/core/common/api/ApiAdjustUserBalance.h>
#include <exchange/core/common/api/ApiBinaryDataCommand.h>
#include <exchange/core/common/api/ApiCancelOrder.h>
#include <exchange/core/common/api/ApiCommand.h>
#include <exchange/core/common/api/ApiMoveOrder.h>
#include <exchange/core/common/api/ApiOrderBookRequest.h>
#include <exchange/core/common/api/ApiPlaceOrder.h>
#include <exchange/core/common/api/ApiReduceOrder.h>
#include <exchange/core/common/cmd/CommandResultCode.h>
#include <exchange/core/common/cmd/OrderCommand.h>
#include <exchange/core/common/cmd/OrderCommandType.h>
#include <stdexcept>
#include <vector>

namespace exchange {
namespace core {

void SimpleEventsProcessor::Accept(common::cmd::OrderCommand *cmd,
                                   int64_t seq) {
  try {
    SendCommandResult(cmd, seq);
    SendTradeEvents(cmd);
    SendMarketData(cmd);
  } catch (const std::exception &ex) {
    // Log error - exception handling
  }
}

void SimpleEventsProcessor::SendApiCommandResult(
    common::api::ApiCommand *apiCmd, common::cmd::CommandResultCode resultCode,
    int64_t timestamp, int64_t seq) {
  apiCmd->timestamp = timestamp;
  ApiCommandResult commandResult{apiCmd, resultCode, seq};
  eventsHandler_->CommandResult(commandResult);
}

void SimpleEventsProcessor::SendCommandResult(common::cmd::OrderCommand *cmd,
                                              int64_t seq) {
  common::api::ApiCommand *apiCmd = nullptr;

  switch (cmd->command) {
  case common::cmd::OrderCommandType::PLACE_ORDER:
    apiCmd = new common::api::ApiPlaceOrder(
        cmd->price, cmd->size, cmd->orderId, cmd->action, cmd->orderType,
        cmd->uid, cmd->symbol, cmd->userCookie, cmd->reserveBidPrice);
    break;

  case common::cmd::OrderCommandType::MOVE_ORDER:
    apiCmd = new common::api::ApiMoveOrder(cmd->orderId, cmd->price, cmd->uid,
                                           cmd->symbol);
    break;

  case common::cmd::OrderCommandType::CANCEL_ORDER:
    apiCmd =
        new common::api::ApiCancelOrder(cmd->orderId, cmd->uid, cmd->symbol);
    break;

  case common::cmd::OrderCommandType::REDUCE_ORDER:
    apiCmd = new common::api::ApiReduceOrder(cmd->orderId, cmd->uid,
                                             cmd->symbol, cmd->size);
    break;

  case common::cmd::OrderCommandType::ADD_USER:
    apiCmd = new common::api::ApiAddUser(cmd->uid);
    break;

  case common::cmd::OrderCommandType::BALANCE_ADJUSTMENT:
    apiCmd = new common::api::ApiAdjustUserBalance(cmd->uid, cmd->symbol,
                                                   cmd->price, cmd->orderId);
    break;

  case common::cmd::OrderCommandType::BINARY_DATA_COMMAND:
    if (cmd->resultCode != common::cmd::CommandResultCode::ACCEPTED) {
      apiCmd = new common::api::ApiBinaryDataCommand(cmd->userCookie, nullptr);
    }
    break;

  case common::cmd::OrderCommandType::ORDER_BOOK_REQUEST:
    apiCmd = new common::api::ApiOrderBookRequest(
        cmd->symbol, static_cast<int32_t>(cmd->size));
    break;

  default:
    // Other commands not handled
    break;
  }

  if (apiCmd != nullptr) {
    SendApiCommandResult(apiCmd, cmd->resultCode, cmd->timestamp, seq);
    delete apiCmd; // Clean up
  }
}

void SimpleEventsProcessor::SendTradeEvents(common::cmd::OrderCommand *cmd) {
  common::MatcherTradeEvent *firstEvent = cmd->matcherEvent;
  if (firstEvent == nullptr) {
    return;
  }

  if (firstEvent->eventType == common::MatcherEventType::REDUCE) {
    ReduceEvent evt{
        cmd->symbol,       firstEvent->size, firstEvent->activeOrderCompleted,
        firstEvent->price, cmd->orderId,     cmd->uid,
        cmd->timestamp};
    eventsHandler_->ReduceEvent(evt);

    if (firstEvent->nextEvent != nullptr) {
      // Only single REDUCE event is expected
      throw std::runtime_error("Only single REDUCE event is expected");
    }
    return;
  }

  SendTradeEvent(cmd);
}

void SimpleEventsProcessor::SendTradeEvent(common::cmd::OrderCommand *cmd) {
  bool takerOrderCompleted = false;
  int64_t totalVolume = 0;
  std::vector<Trade> trades;

  RejectEvent *rejectEvent = nullptr;

  // Process all matcher events
  common::MatcherTradeEvent *event = cmd->matcherEvent;
  while (event != nullptr) {
    if (event->eventType == common::MatcherEventType::TRADE) {
      Trade trade{event->matchedOrderId, event->matchedOrderUid,
                  event->matchedOrderCompleted, event->price, event->size};
      trades.push_back(trade);
      totalVolume += event->size;

      if (event->activeOrderCompleted) {
        takerOrderCompleted = true;
      }
    } else if (event->eventType == common::MatcherEventType::REJECT) {
      rejectEvent = new RejectEvent{cmd->symbol,  event->size, event->price,
                                    cmd->orderId, cmd->uid,    cmd->timestamp};
    }

    event = event->nextEvent;
  }

  if (!trades.empty()) {
    TradeEvent evt{cmd->symbol, totalVolume,         cmd->orderId,   cmd->uid,
                   cmd->action, takerOrderCompleted, cmd->timestamp, trades};
    eventsHandler_->TradeEvent(evt);
  }

  if (rejectEvent != nullptr) {
    eventsHandler_->RejectEvent(*rejectEvent);
    delete rejectEvent;
  }
}

void SimpleEventsProcessor::SendMarketData(common::cmd::OrderCommand *cmd) {
  // With shared_ptr, we can simply copy the shared_ptr (matching Java behavior)
  // No need for special handling for ORDER_BOOK_REQUEST since shared_ptr can be
  // copied
  if (cmd->marketData == nullptr) {
    return;
  }

  // Copy shared_ptr (cheap operation, just increments reference count)
  std::shared_ptr<common::L2MarketData> marketData = cmd->marketData;

  std::vector<OrderBookRecord> asks;
  asks.reserve(marketData->askSize);
  for (int32_t i = 0; i < marketData->askSize; i++) {
    asks.push_back(
        OrderBookRecord{marketData->askPrices[i], marketData->askVolumes[i],
                        static_cast<int32_t>(marketData->askOrders[i])});
  }

  std::vector<OrderBookRecord> bids;
  bids.reserve(marketData->bidSize);
  for (int32_t i = 0; i < marketData->bidSize; i++) {
    bids.push_back(
        OrderBookRecord{marketData->bidPrices[i], marketData->bidVolumes[i],
                        static_cast<int32_t>(marketData->bidOrders[i])});
  }

  OrderBook orderBook{cmd->symbol, asks, bids, cmd->timestamp};
  eventsHandler_->OrderBook(orderBook);
}

} // namespace core
} // namespace exchange
