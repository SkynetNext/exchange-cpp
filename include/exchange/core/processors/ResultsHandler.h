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
#include <functional>
#include "../common/cmd/OrderCommand.h"
#include "../common/cmd/OrderCommandType.h"

namespace exchange::core::processors {

/**
 * ResultsHandler - handles results from the pipeline
 */
class ResultsHandler {
public:
  using ResultsConsumer = std::function<void(common::cmd::OrderCommand*, int64_t)>;

  explicit ResultsHandler(ResultsConsumer resultsConsumer)
    : resultsConsumer_(std::move(resultsConsumer)) {}

  void OnEvent(common::cmd::OrderCommand* cmd, int64_t sequence, bool endOfBatch) {
    if (cmd->command == common::cmd::OrderCommandType::GROUPING_CONTROL) {
      processingEnabled_ = (cmd->orderId == 1);
    }

    if (processingEnabled_) {
      resultsConsumer_(cmd, sequence);
    }
  }

private:
  ResultsConsumer resultsConsumer_;
  bool processingEnabled_ = true;
};

}  // namespace exchange::core::processors
