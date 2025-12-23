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

#include <exchange/core/collections/objpool/ObjectsPool.h>
#include <exchange/core/common/cmd/OrderCommand.h>
#include <exchange/core/common/cmd/OrderCommandType.h>
#include <exchange/core/orderbook/IOrderBook.h>
#include <exchange/core/orderbook/OrderBookEventsHelper.h>
#include <exchange/core/orderbook/OrderBookNaiveImpl.h>
#include <exchange/core/processors/MatchingEngineRouter.h>
#include <exchange/core/processors/SharedPool.h>
#include <stdexcept>

namespace exchange {
namespace core {
namespace processors {

MatchingEngineRouter::MatchingEngineRouter(
    int32_t shardId, int64_t numShards, OrderBookFactory orderBookFactory,
    SharedPool *sharedPool, SymbolSpecificationProvider *symbolSpecProvider)
    : shardId_(shardId), shardMask_(numShards - 1),
      symbolSpecProvider_(symbolSpecProvider),
      orderBookFactory_(std::move(orderBookFactory)),
      cfgMarginTradingEnabled_(false), cfgSendL2ForEveryCmd_(false),
      cfgL2RefreshDepth_(8) {

  if ((numShards & (numShards - 1)) != 0) {
    throw std::invalid_argument(
        "Invalid number of shards - must be power of 2");
  }

  // Create OrderBookEventsHelper with SharedPool
  if (sharedPool != nullptr) {
    eventsHelper_ = std::make_unique<orderbook::OrderBookEventsHelper>(
        [sharedPool]() { return sharedPool->GetChain(); });
  } else {
    eventsHelper_ = std::make_unique<orderbook::OrderBookEventsHelper>();
  }

  // Initialize object pools
  // Matches Java MatchingEngineRouter configuration (production pool)
  // TODO: Move to performance configuration
  objectsPool_ =
      std::unique_ptr<::exchange::core::collections::objpool::ObjectsPool>(
          ::exchange::core::collections::objpool::ObjectsPool::
              CreateProductionPool());
}

void MatchingEngineRouter::ProcessOrder(int64_t seq,
                                        common::cmd::OrderCommand *cmd) {
  if (cmd->command == common::cmd::OrderCommandType::RESET) {
    Reset();
    return;
  }

  if (cmd->command == common::cmd::OrderCommandType::BINARY_DATA_COMMAND ||
      cmd->command == common::cmd::OrderCommandType::BINARY_DATA_QUERY) {
    // Binary commands are handled by BinaryCommandsProcessor
    return;
  }

  // Route to appropriate order book
  orderbook::IOrderBook *orderBook = GetOrderBook(cmd->symbol);
  if (orderBook == nullptr) {
    cmd->resultCode =
        common::cmd::CommandResultCode::MATCHING_INVALID_ORDER_BOOK_ID;
    return;
  }

  // Process command
  orderbook::IOrderBook::ProcessCommand(orderBook, cmd);
}

orderbook::IOrderBook *MatchingEngineRouter::GetOrderBook(int32_t symbolId) {
  auto it = orderBooks_.find(symbolId);
  return (it != orderBooks_.end()) ? it->second.get() : nullptr;
}

void MatchingEngineRouter::AddSymbol(
    const common::CoreSymbolSpecification *spec) {
  if (spec == nullptr) {
    return;
  }

  if (orderBooks_.find(spec->symbolId) != orderBooks_.end()) {
    // Symbol already exists
    return;
  }

  // Create new order book using factory
  if (orderBookFactory_) {
    auto orderBook =
        orderBookFactory_(spec, objectsPool_.get(), eventsHelper_.get());
    orderBooks_[spec->symbolId] = std::move(orderBook);
  } else {
    // Fallback to naive implementation
    auto orderBook = std::make_unique<orderbook::OrderBookNaiveImpl>(
        spec, objectsPool_.get(), eventsHelper_.get());
    orderBooks_[spec->symbolId] = std::move(orderBook);
  }

  if (symbolSpecProvider_ != nullptr) {
    symbolSpecProvider_->AddSymbol(spec);
  }
}

bool MatchingEngineRouter::SymbolForThisHandler(int32_t symbolId) const {
  return (symbolId & shardMask_) == shardId_;
}

void MatchingEngineRouter::Reset() {
  orderBooks_.clear();
  if (symbolSpecProvider_ != nullptr) {
    symbolSpecProvider_->Reset();
  }
}

std::vector<orderbook::IOrderBook *>
MatchingEngineRouter::GetOrderBooks() const {
  std::vector<orderbook::IOrderBook *> result;
  result.reserve(orderBooks_.size());
  for (const auto &pair : orderBooks_) {
    result.push_back(pair.second.get());
  }
  return result;
}

void MatchingEngineRouter::ProcessMatchingCommand(
    common::cmd::OrderCommand *cmd) {
  orderbook::IOrderBook *orderBook = GetOrderBook(cmd->symbol);
  if (orderBook == nullptr) {
    cmd->resultCode =
        common::cmd::CommandResultCode::MATCHING_INVALID_ORDER_BOOK_ID;
    return;
  }

  // Process command using static helper
  orderbook::IOrderBook::ProcessCommand(orderBook, cmd);

  // Post market data if needed
  if ((cfgSendL2ForEveryCmd_ || (cmd->serviceFlags & 1) != 0) &&
      cmd->command != common::cmd::OrderCommandType::ORDER_BOOK_REQUEST &&
      cmd->resultCode == common::cmd::CommandResultCode::SUCCESS) {
    cmd->marketData = orderBook->GetL2MarketDataSnapshot(cfgL2RefreshDepth_);
  }
}

} // namespace processors
} // namespace core
} // namespace exchange
