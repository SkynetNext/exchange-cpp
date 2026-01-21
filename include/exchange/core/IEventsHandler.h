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
#include <vector>
#include "common/OrderAction.h"
#include "common/api/ApiCommand.h"
#include "common/cmd/CommandResultCode.h"

namespace exchange::core {

// Forward declarations
struct ApiCommandResult;
struct TradeEvent;
struct RejectEvent;
struct ReduceEvent;
struct OrderBook;

/**
 * IEventsHandler - convenient events handler interface
 * Handler methods are invoked from single thread in following order:
 * 1. commandResult
 * 2A. optional reduceEvent / optional tradeEvent
 * 2B. optional rejectEvent
 * 3. orderBook - mandatory for ApiOrderBookRequest, optional for other commands
 */
class IEventsHandler {
 public:
    virtual ~IEventsHandler() = default;

    /**
     * Method is called after each commands execution.
     */
    virtual void CommandResult(const ApiCommandResult& commandResult) = 0;

    /**
     * Method is called if order execution was resulted to one or more trades.
     */
    virtual void TradeEvent(const TradeEvent& tradeEvent) = 0;

    /**
     * Method is called if IoC order was not possible to match with provided price
     * limit.
     */
    virtual void RejectEvent(const RejectEvent& rejectEvent) = 0;

    /**
     * Method is called if Cancel or Reduce command was successfully executed.
     */
    virtual void ReduceEvent(const ReduceEvent& reduceEvent) = 0;

    /**
     * Method is called when order book snapshot (L2MarketData) was attached to
     * commands by matching engine.
     */
    virtual void OrderBook(const OrderBook& orderBook) = 0;
};

// Event data structures
struct ApiCommandResult {
    const common::api::ApiCommand* command;
    common::cmd::CommandResultCode resultCode;
    int64_t seq;
};

struct Trade {
    int64_t makerOrderId;
    int64_t makerUid;
    bool makerOrderCompleted;
    int64_t price;
    int64_t volume;
};

struct TradeEvent {
    int32_t symbol;
    int64_t totalVolume;
    int64_t takerOrderId;
    int64_t takerUid;
    common::OrderAction takerAction;
    bool takeOrderCompleted;
    int64_t timestamp;
    std::vector<Trade> trades;
};

struct ReduceEvent {
    int32_t symbol;
    int64_t reducedVolume;
    bool orderCompleted;
    int64_t price;
    int64_t orderId;
    int64_t uid;
    int64_t timestamp;
};

struct RejectEvent {
    int32_t symbol;
    int64_t rejectedVolume;
    int64_t price;
    int64_t orderId;
    int64_t uid;
    int64_t timestamp;
};

struct OrderBookRecord {
    int64_t price;
    int64_t volume;
    int32_t orders;
};

struct OrderBook {
    int32_t symbol;
    std::vector<OrderBookRecord> asks;
    std::vector<OrderBookRecord> bids;
    int64_t timestamp;
};

}  // namespace exchange::core
