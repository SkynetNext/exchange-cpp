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

#include <ankerl/unordered_dense.h>
#include <cstdint>
#include <functional>
#include <memory>
#include "../common/StateHash.h"
#include "../common/WriteBytesMarshallable.h"
#include "../common/api/binary/BinaryDataCommand.h"
#include "../common/api/reports/ReportQueriesHandler.h"
#include "../common/config/ReportsQueriesConfiguration.h"
#include "../orderbook/OrderBookEventsHelper.h"
#include "SharedPool.h"

namespace exchange::core {
namespace common {
class BytesIn;

namespace cmd {
class OrderCommand;
}
}  // namespace common

namespace processors {

/**
 * BinaryCommandsProcessor - stateful binary commands processor
 * Handles binary data commands and report queries
 */
class BinaryCommandsProcessor : public common::StateHash, public common::WriteBytesMarshallable {
public:
  using CompleteMessagesHandler = std::function<void(common::api::binary::BinaryDataCommand*)>;

  BinaryCommandsProcessor(CompleteMessagesHandler completeMessagesHandler,
                          common::api::reports::ReportQueriesHandler* reportQueriesHandler,
                          SharedPool* sharedPool,
                          const common::config::ReportsQueriesConfiguration* queriesConfiguration,
                          int32_t section);

  /**
   * Constructor from BytesIn (deserialization)
   */
  BinaryCommandsProcessor(CompleteMessagesHandler completeMessagesHandler,
                          common::api::reports::ReportQueriesHandler* reportQueriesHandler,
                          SharedPool* sharedPool,
                          const common::config::ReportsQueriesConfiguration* queriesConfiguration,
                          common::BytesIn* bytesIn,
                          int32_t section);

  /**
   * Accept binary frame from OrderCommand
   */
  common::cmd::CommandResultCode AcceptBinaryFrame(common::cmd::OrderCommand* cmd);

  /**
   * Reset - clear all state
   */
  void Reset();

  // StateHash interface
  int32_t GetStateHash() const override;

  // WriteBytesMarshallable interface
  void WriteMarshallable(common::BytesOut& bytes) const override;

private:
  // transactionId -> TransferRecord (simplified for now)
  ankerl::unordered_dense::map<int64_t, void*> incomingData_;

  CompleteMessagesHandler completeMessagesHandler_;
  common::api::reports::ReportQueriesHandler* reportQueriesHandler_;
  std::unique_ptr<orderbook::OrderBookEventsHelper> eventsHelper_;
  const common::config::ReportsQueriesConfiguration* queriesConfiguration_;
  int32_t section_;
};

}  // namespace processors
}  // namespace exchange::core
