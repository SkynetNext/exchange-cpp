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
#include <future>
#include <tbb/concurrent_hash_map.h>
#include <vector>

// Include RingBuffer to use MultiProducerRingBuffer type alias
#include <disruptor/RingBuffer.h>

namespace exchange {
namespace core {

// Forward declarations
namespace common {
namespace api {
class ApiBinaryDataCommand;
class ApiPersistState;
} // namespace api
} // namespace common

/**
 * IExchangeApi - non-template interface for ExchangeApi
 */
class IExchangeApi {
public:
  virtual ~IExchangeApi() = default;

  /**
   * Submit command (fire and forget)
   */
  virtual void SubmitCommand(common::api::ApiCommand *cmd) = 0;

  /**
   * Submit command async (returns future)
   */
  virtual std::future<common::cmd::CommandResultCode>
  SubmitCommandAsync(common::api::ApiCommand *cmd) = 0;

  /**
   * Submit commands synchronously
   */
  virtual void
  SubmitCommandsSync(const std::vector<common::api::ApiCommand *> &cmds) = 0;

  /**
   * Process result from pipeline
   */
  virtual void ProcessResult(int64_t seq, common::cmd::OrderCommand *cmd) = 0;
};

/**
 * ExchangeApi - main API interface for submitting commands
 * Uses MultiProducerRingBuffer (most common case)
 */
template <typename WaitStrategyT> class ExchangeApi : public IExchangeApi {
public:
  using ResultsConsumer =
      std::function<void(common::cmd::OrderCommand *, int64_t)>;

  explicit ExchangeApi(disruptor::MultiProducerRingBuffer<
                       common::cmd::OrderCommand, WaitStrategyT> *ringBuffer);

  /**
   * Process result from pipeline
   */
  void ProcessResult(int64_t seq, common::cmd::OrderCommand *cmd) override;

  /**
   * Submit command (fire and forget)
   */
  void SubmitCommand(common::api::ApiCommand *cmd) override;

  /**
   * Submit command async (returns future)
   */
  std::future<common::cmd::CommandResultCode>
  SubmitCommandAsync(common::api::ApiCommand *cmd) override;

  /**
   * Submit commands synchronously
   */
  void SubmitCommandsSync(
      const std::vector<common::api::ApiCommand *> &cmds) override;

private:
  disruptor::MultiProducerRingBuffer<common::cmd::OrderCommand, WaitStrategyT>
      *ringBuffer_;

  // promises cache (seq -> promise)
  // Thread-safe: SubmitCommandAsync (main thread) and ProcessResult
  // (ResultsHandler thread) may access concurrently
  // Using Intel TBB concurrent_hash_map (lock-free, better than Java
  // ConcurrentHashMap) - header-only, no library linking needed
  using PromiseMap =
      tbb::concurrent_hash_map<int64_t,
                               std::promise<common::cmd::CommandResultCode>>;
  PromiseMap promises_;

  void PublishCommand(common::api::ApiCommand *cmd, int64_t seq);

  // Batch publishing methods (using next(n) + publish(lo, hi))
  void PublishBinaryData(common::api::ApiBinaryDataCommand *apiCmd,
                         std::function<void(int64_t)> endSeqConsumer);
  void PublishPersistCmd(common::api::ApiPersistState *api,
                         std::function<void(int64_t, int64_t)> seqConsumer);

  static constexpr int LONGS_PER_MESSAGE = 5;
};

} // namespace core
} // namespace exchange
