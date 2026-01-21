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

#include <exchange/core/common/cmd/OrderCommand.h>
#include <cstdint>
#include <vector>

namespace exchange::core::common::cmd {

OrderCommand OrderCommand::NewOrder(OrderType orderType,
                                    int64_t orderId,
                                    int64_t uid,
                                    int64_t price,
                                    int64_t reserveBidPrice,
                                    int64_t size,
                                    OrderAction action) {
  OrderCommand cmd;
  cmd.command = OrderCommandType::PLACE_ORDER;
  cmd.orderId = orderId;
  cmd.uid = uid;
  cmd.price = price;
  cmd.reserveBidPrice = reserveBidPrice;
  cmd.size = size;
  cmd.action = action;
  cmd.orderType = orderType;
  cmd.resultCode = CommandResultCode::VALID_FOR_MATCHING_ENGINE;
  return cmd;
}

OrderCommand OrderCommand::Cancel(int64_t orderId, int64_t uid) {
  OrderCommand cmd;
  cmd.command = OrderCommandType::CANCEL_ORDER;
  cmd.orderId = orderId;
  cmd.uid = uid;
  cmd.resultCode = CommandResultCode::VALID_FOR_MATCHING_ENGINE;
  return cmd;
}

OrderCommand OrderCommand::Reduce(int64_t orderId, int64_t uid, int64_t reduceSize) {
  OrderCommand cmd;
  cmd.command = OrderCommandType::REDUCE_ORDER;
  cmd.orderId = orderId;
  cmd.uid = uid;
  cmd.size = reduceSize;
  cmd.resultCode = CommandResultCode::VALID_FOR_MATCHING_ENGINE;
  return cmd;
}

OrderCommand OrderCommand::Update(int64_t orderId, int64_t uid, int64_t price) {
  OrderCommand cmd;
  cmd.command = OrderCommandType::MOVE_ORDER;
  cmd.orderId = orderId;
  cmd.uid = uid;
  cmd.price = price;
  cmd.resultCode = CommandResultCode::VALID_FOR_MATCHING_ENGINE;
  return cmd;
}

std::vector<MatcherTradeEvent*> OrderCommand::ExtractEvents() const {
  std::vector<MatcherTradeEvent*> list;
  ProcessMatcherEvents([&list](MatcherTradeEvent* event) { list.push_back(event); });
  return list;
}

void OrderCommand::WriteTo(OrderCommand& cmd2) const {
  cmd2.command = this->command;
  cmd2.orderId = this->orderId;
  cmd2.symbol = this->symbol;
  cmd2.uid = this->uid;
  cmd2.timestamp = this->timestamp;
  cmd2.reserveBidPrice = this->reserveBidPrice;
  cmd2.price = this->price;
  cmd2.size = this->size;
  cmd2.action = this->action;
  cmd2.orderType = this->orderType;
}

OrderCommand OrderCommand::Copy() const {
  OrderCommand newCmd;
  WriteTo(newCmd);
  newCmd.resultCode = this->resultCode;

  auto events = ExtractEvents();
  for (auto it = events.rbegin(); it != events.rend(); ++it) {
    MatcherTradeEvent* copy = new MatcherTradeEvent(**it);
    copy->nextEvent = newCmd.matcherEvent;
    newCmd.matcherEvent = copy;
  }

  if (marketData != nullptr) {
    // Match Java: marketData.copy() - create a new copy
    // Since we're using shared_ptr, we need to create a new L2MarketData
    // instance
    auto copy = marketData->Copy();
    newCmd.marketData = std::shared_ptr<L2MarketData>(copy.release());
  }

  return newCmd;
}

}  // namespace exchange::core::common::cmd
