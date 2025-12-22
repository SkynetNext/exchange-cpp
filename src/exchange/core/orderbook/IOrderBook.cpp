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
#include <exchange/core/orderbook/IOrderBook.h>

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

} // namespace orderbook
} // namespace core
} // namespace exchange
