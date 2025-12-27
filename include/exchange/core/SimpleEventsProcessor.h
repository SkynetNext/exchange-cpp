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

#include "IEventsHandler.h"
#include "common/cmd/OrderCommand.h"
#include <cstdint>
#include <functional>

namespace exchange {
namespace core {

/**
 * SimpleEventsProcessor - processes events and calls IEventsHandler
 */
class SimpleEventsProcessor {
public:
  using ResultsConsumer =
      std::function<void(common::cmd::OrderCommand *, int64_t)>;

  explicit SimpleEventsProcessor(IEventsHandler *eventsHandler)
      : eventsHandler_(eventsHandler) {}

  void Accept(common::cmd::OrderCommand *cmd, int64_t seq);

private:
  IEventsHandler *eventsHandler_;

  void SendCommandResult(common::cmd::OrderCommand *cmd, int64_t seq);
  void SendTradeEvents(common::cmd::OrderCommand *cmd);
  void SendMarketData(common::cmd::OrderCommand *cmd);
  void SendTradeEvent(common::cmd::OrderCommand *cmd);
  void SendApiCommandResult(common::api::ApiCommand *cmd,
                            common::cmd::CommandResultCode resultCode,
                            int64_t timestamp, int64_t seq);
};

} // namespace core
} // namespace exchange
