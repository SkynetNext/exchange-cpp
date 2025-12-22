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

#pragma once

#include "common/api/ApiCommand.h"
#include "common/cmd/OrderCommand.h"
#include <ankerl/unordered_dense.h>
#include <cstdint>
#include <functional>
#include <vector>

// Include RingBuffer to use MultiProducerRingBuffer type alias
#include <disruptor/RingBuffer.h>

namespace exchange {
namespace core {

/**
 * ExchangeApi - main API interface for submitting commands
 * Uses MultiProducerRingBuffer (most common case)
 */
template <typename WaitStrategyT> class ExchangeApi {
public:
  using ResultsConsumer =
      std::function<void(common::cmd::OrderCommand *, int64_t)>;

  explicit ExchangeApi(disruptor::MultiProducerRingBuffer<
                       common::cmd::OrderCommand, WaitStrategyT> *ringBuffer);

  /**
   * Process result from pipeline
   */
  void ProcessResult(int64_t seq, common::cmd::OrderCommand *cmd);

  /**
   * Submit command (fire and forget)
   */
  void SubmitCommand(common::api::ApiCommand *cmd);

  /**
   * Submit command async (returns future)
   */
  // TODO: Implement async version with std::future

  /**
   * Submit commands synchronously
   */
  void SubmitCommandsSync(const std::vector<common::api::ApiCommand *> &cmds);

private:
  disruptor::MultiProducerRingBuffer<common::cmd::OrderCommand, WaitStrategyT>
      *ringBuffer_;

  // promises cache (seq -> consumer)
  ankerl::unordered_dense::map<int64_t, ResultsConsumer> promises_;

  void PublishCommand(common::api::ApiCommand *cmd, int64_t seq);
};

} // namespace core
} // namespace exchange
