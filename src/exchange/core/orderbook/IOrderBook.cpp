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

#include <climits>
#include <exchange/core/collections/objpool/ObjectsPool.h>
#include <exchange/core/common/BytesIn.h>
#include <exchange/core/common/config/LoggingConfiguration.h>
#include <exchange/core/orderbook/IOrderBook.h>
#include <exchange/core/orderbook/OrderBookDirectImpl.h>
#include <exchange/core/orderbook/OrderBookEventsHelper.h>
#include <exchange/core/orderbook/OrderBookNaiveImpl.h>
#include <stdexcept>

namespace exchange {
namespace core {
namespace orderbook {

common::cmd::CommandResultCode
IOrderBook::ProcessCommand(IOrderBook *orderBook,
                           common::cmd::OrderCommand *cmd) {
  common::cmd::OrderCommandType commandType = cmd->command;

  if (commandType == common::cmd::OrderCommandType::MOVE_ORDER) {
    return orderBook->MoveOrder(cmd);
  } else if (commandType == common::cmd::OrderCommandType::CANCEL_ORDER) {
    return orderBook->CancelOrder(cmd);
  } else if (commandType == common::cmd::OrderCommandType::REDUCE_ORDER) {
    return orderBook->ReduceOrder(cmd);
  } else if (commandType == common::cmd::OrderCommandType::PLACE_ORDER) {
    if (cmd->resultCode ==
        common::cmd::CommandResultCode::VALID_FOR_MATCHING_ENGINE) {
      orderBook->NewOrder(cmd);
      return common::cmd::CommandResultCode::SUCCESS;
    } else {
      return cmd->resultCode; // no change
    }
  } else if (commandType == common::cmd::OrderCommandType::ORDER_BOOK_REQUEST) {
    int32_t size = static_cast<int32_t>(cmd->size);
    cmd->marketData =
        orderBook->GetL2MarketDataSnapshot(size >= 0 ? size : INT_MAX);
    return common::cmd::CommandResultCode::SUCCESS;
  } else {
    return common::cmd::CommandResultCode::MATCHING_UNSUPPORTED_COMMAND;
  }
}

std::unique_ptr<IOrderBook> IOrderBook::Create(
    common::BytesIn *bytes,
    ::exchange::core::collections::objpool::ObjectsPool *objectsPool,
    OrderBookEventsHelper *eventsHelper,
    const common::config::LoggingConfiguration *loggingCfg) {
  if (bytes == nullptr) {
    throw std::invalid_argument("BytesIn cannot be nullptr");
  }

  // Read implementation type
  int8_t implTypeCode = bytes->ReadByte();
  OrderBookImplType implType = static_cast<OrderBookImplType>(implTypeCode);

  switch (implType) {
  case OrderBookImplType::NAIVE:
    return std::make_unique<OrderBookNaiveImpl>(bytes, loggingCfg);
  case OrderBookImplType::DIRECT:
    return std::make_unique<OrderBookDirectImpl>(bytes, objectsPool,
                                                 eventsHelper, loggingCfg);
  default:
    throw std::invalid_argument("Unknown OrderBook implementation type: " +
                                std::to_string(implTypeCode));
  }
}

} // namespace orderbook
} // namespace core
} // namespace exchange
