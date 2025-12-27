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

#include "common/BytesIn.h"
#include "common/VectorBytesIn.h"
#include "common/VectorBytesOut.h"
#include "common/api/ApiCommand.h"
#include "common/cmd/OrderCommand.h"
#include <ankerl/unordered_dense.h>
#include <cstdint>
#include <functional>
#include <future>
#include <memory>
#include <tbb/concurrent_hash_map.h>
#include <vector>

// Include RingBuffer to use MultiProducerRingBuffer type alias
#include <disruptor/RingBuffer.h>
// Forward declarations for wait strategies (needed for ProcessReport in
// IExchangeApi)
#include <disruptor/BlockingWaitStrategy.h>
#include <disruptor/BusySpinWaitStrategy.h>
#include <disruptor/YieldingWaitStrategy.h>

namespace exchange {
namespace core {

// Forward declarations
namespace common {
namespace api {
class ApiBinaryDataCommand;
class ApiPersistState;
namespace reports {
class ApiReportQuery;
template <typename T> class ReportQuery;
class ReportResult;
} // namespace reports
} // namespace api
} // namespace common

// Forward declaration for ExchangeApi template
template <typename WaitStrategyT> class ExchangeApi;

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
   * Submit command async with full response (returns OrderCommand future)
   * Matches Java submitCommandAsyncFullResponse
   */
  virtual std::future<common::cmd::OrderCommand>
  SubmitCommandAsyncFullResponse(common::api::ApiCommand *cmd) = 0;

  /**
   * Submit commands synchronously
   */
  virtual void
  SubmitCommandsSync(const std::vector<common::api::ApiCommand *> &cmds) = 0;

  /**
   * Process result from pipeline
   */
  virtual void ProcessResult(int64_t seq, common::cmd::OrderCommand *cmd) = 0;

  /**
   * Process report query - type-erased virtual function
   * This allows calling ProcessReport through IExchangeApi* without knowing
   * the concrete WaitStrategy type. Uses type erasure to handle different
   * ReportQuery types.
   *
   * @param queryTypeId Report type code (from ReportQuery::GetReportTypeCode())
   * @param queryBytes Serialized query bytes (includes type code + query data)
   * @param transferId Transfer ID for binary data
   * @return Future with serialized result sections (one per shard)
   */
  virtual std::future<std::vector<std::vector<uint8_t>>>
  ProcessReportAny(int32_t queryTypeId, std::vector<uint8_t> queryBytes,
                   int32_t transferId) = 0;

  /**
   * Request order book snapshot async (matches Java requestOrderBookAsync)
   * @param symbolId Symbol ID
   * @param depth Maximum depth of order book
   * @return Future with L2MarketData (shared_ptr to match
   * OrderCommand::marketData)
   */
  virtual std::future<std::shared_ptr<common::L2MarketData>>
  RequestOrderBookAsync(int32_t symbolId, int32_t depth) = 0;
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
   * Submit command async with full response (returns OrderCommand future)
   * Matches Java submitCommandAsyncFullResponse
   */
  std::future<common::cmd::OrderCommand>
  SubmitCommandAsyncFullResponse(common::api::ApiCommand *cmd) override;

  /**
   * Submit commands synchronously
   */
  void SubmitCommandsSync(
      const std::vector<common::api::ApiCommand *> &cmds) override;

  /**
   * Process report query (matches Java processReport)
   * @tparam Q ReportQuery type
   * @tparam R ReportResult type
   * @param query Report query to process
   * @param transferId Transfer ID for binary data
   * @return Future with report result
   */
  template <typename Q, typename R>
  std::future<std::unique_ptr<R>> ProcessReport(std::unique_ptr<Q> query,
                                                int32_t transferId);

  /**
   * Process report query - type-erased virtual function implementation
   * Serializes query, calls internal ProcessReport, returns serialized sections
   */
  std::future<std::vector<std::vector<uint8_t>>>
  ProcessReportAny(int32_t queryTypeId, std::vector<uint8_t> queryBytes,
                   int32_t transferId) override;

  /**
   * Request order book snapshot async (matches Java requestOrderBookAsync)
   * @param symbolId Symbol ID
   * @param depth Maximum depth of order book
   * @return Future with L2MarketData (shared_ptr to match
   * OrderCommand::marketData)
   */
  std::future<std::shared_ptr<common::L2MarketData>>
  RequestOrderBookAsync(int32_t symbolId, int32_t depth) override;

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

  // Report result promises cache (seq -> promise for report result)
  // Used for ProcessReport to extract results from OrderCommand
  using ReportResultPromise = std::function<void(common::cmd::OrderCommand *)>;
  using ReportPromiseMap =
      tbb::concurrent_hash_map<int64_t, ReportResultPromise>;
  ReportPromiseMap reportPromises_;

  // Order book request promises cache (seq -> promise for L2MarketData)
  // Used for RequestOrderBookAsync to extract market data from OrderCommand
  using OrderBookPromiseMap = tbb::concurrent_hash_map<
      int64_t, std::promise<std::shared_ptr<common::L2MarketData>>>;
  OrderBookPromiseMap orderBookPromises_;

  // Full response promises cache (seq -> promise for OrderCommand)
  // Used for SubmitCommandAsyncFullResponse to return complete OrderCommand
  using FullResponsePromiseMap =
      tbb::concurrent_hash_map<int64_t,
                               std::promise<common::cmd::OrderCommand>>;
  FullResponsePromiseMap fullResponsePromises_;

  void PublishCommand(common::api::ApiCommand *cmd, int64_t seq);

  // Batch publishing methods (using next(n) + publish(lo, hi))
  void PublishBinaryData(common::api::ApiBinaryDataCommand *apiCmd,
                         std::function<void(int64_t)> endSeqConsumer);
  void PublishPersistCmd(common::api::ApiPersistState *api,
                         std::function<void(int64_t, int64_t)> seqConsumer);
  void PublishQuery(common::api::reports::ApiReportQuery *apiCmd,
                    std::function<void(int64_t)> endSeqConsumer);

  static constexpr int LONGS_PER_MESSAGE = 5;
};

/**
 * Helper function to call ProcessReport through IExchangeApi*
 * Uses virtual function dispatch (no dynamic_cast or static_cast needed!)
 * This is the proper way to call ProcessReport through IExchangeApi*
 *
 * @tparam Q ReportQuery type
 * @tparam R ReportResult type
 * @param api IExchangeApi pointer
 * @param query Report query to process
 * @param transferId Transfer ID for binary data
 * @return Future with report result
 */
template <typename Q, typename R>
std::future<std::unique_ptr<R>> ProcessReportHelper(IExchangeApi *api,
                                                    std::unique_ptr<Q> query,
                                                    int32_t transferId) {
  // Serialize query to bytes
  std::vector<uint8_t> queryBytes;
  queryBytes.reserve(128);
  common::VectorBytesOut bytesOut(queryBytes);
  bytesOut.WriteInt(query->GetReportTypeCode());
  query->WriteMarshallable(bytesOut);

  // Call virtual ProcessReportAny method (no type conversion needed!)
  auto sectionsFuture = api->ProcessReportAny(
      query->GetReportTypeCode(), std::move(queryBytes), transferId);

  // Wait for result sections
  auto sections = sectionsFuture.get();

  // Convert sections to BytesIn pointers for CreateResult
  // Skip empty sections (matches Java behavior where empty sections are
  // filtered)
  std::vector<common::BytesIn *> sectionBytes;
  sectionBytes.reserve(sections.size());
  std::vector<std::unique_ptr<common::VectorBytesIn>> sectionBytesOwners;
  sectionBytesOwners.reserve(sections.size());

  for (const auto &section : sections) {
    if (!section.empty()) {
      sectionBytesOwners.push_back(
          std::make_unique<common::VectorBytesIn>(section));
      sectionBytes.push_back(sectionBytesOwners.back().get());
    }
  }

  // Call CreateResult on the original query to merge sections
  auto result = query->CreateResult(sectionBytes);

  // Create promise and set value
  std::promise<std::unique_ptr<R>> promise;
  auto future = promise.get_future();
  promise.set_value(std::unique_ptr<R>(static_cast<R *>(result.release())));

  return future;
}

} // namespace core
} // namespace exchange
