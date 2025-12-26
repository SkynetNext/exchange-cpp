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

#include <disruptor/EventTranslatorOneArg.h>
#include <disruptor/RingBuffer.h>
#include <exchange/core/ExchangeApi.h>
#include <exchange/core/common/BalanceAdjustmentType.h>
#include <exchange/core/common/BytesOut.h>
#include <exchange/core/common/OrderType.h>
#include <exchange/core/common/VectorBytesIn.h>
#include <exchange/core/common/VectorBytesOut.h>
#include <exchange/core/common/api/ApiAddUser.h>
#include <exchange/core/common/api/ApiAdjustUserBalance.h>
#include <exchange/core/common/api/ApiBinaryDataCommand.h>
#include <exchange/core/common/api/ApiCancelOrder.h>
#include <exchange/core/common/api/ApiCommand.h>
#include <exchange/core/common/api/ApiMoveOrder.h>
#include <exchange/core/common/api/ApiNop.h>
#include <exchange/core/common/api/ApiOrderBookRequest.h>
#include <exchange/core/common/api/ApiPersistState.h>
#include <exchange/core/common/api/ApiPlaceOrder.h>
#include <exchange/core/common/api/ApiReduceOrder.h>
#include <exchange/core/common/api/ApiReset.h>
#include <exchange/core/common/api/ApiResumeUser.h>
#include <exchange/core/common/api/ApiSuspendUser.h>
#include <exchange/core/common/api/binary/BinaryDataCommand.h>
#include <exchange/core/common/api/reports/ApiReportQuery.h>
#include <exchange/core/common/api/reports/ReportQueryFactory.h>
#include <exchange/core/common/api/reports/ReportType.h>
#include <exchange/core/common/cmd/CommandResultCode.h>
#include <exchange/core/common/cmd/OrderCommand.h>
#include <exchange/core/common/cmd/OrderCommandType.h>
#include <exchange/core/orderbook/OrderBookEventsHelper.h>
#include <exchange/core/processors/BinaryCommandsProcessor.h>
#include <exchange/core/utils/Logger.h>
#include <exchange/core/utils/SerializationUtils.h>
#include <chrono>
#include <functional>
#include <stdexcept>
#include <vector>

namespace exchange {
namespace core {

// Event translators - similar to Java version
namespace {
// Place order translator
class NewOrderTranslator
    : public disruptor::EventTranslatorOneArg<common::cmd::OrderCommand,
                                              common::api::ApiPlaceOrder> {
public:
  void translateTo(common::cmd::OrderCommand &cmd, int64_t seq,
                   common::api::ApiPlaceOrder api) override {
    cmd.command = common::cmd::OrderCommandType::PLACE_ORDER;
    cmd.price = api.price;
    cmd.reserveBidPrice = api.reservePrice;
    cmd.size = api.size;
    cmd.orderId = api.orderId;
    cmd.timestamp = api.timestamp;
    cmd.action = api.action;
    cmd.orderType = api.orderType;
    cmd.symbol = api.symbol;
    cmd.uid = api.uid;
    cmd.userCookie = api.userCookie;
    cmd.resultCode = common::cmd::CommandResultCode::NEW;
  }
};

// Move order translator
class MoveOrderTranslator
    : public disruptor::EventTranslatorOneArg<common::cmd::OrderCommand,
                                              common::api::ApiMoveOrder> {
public:
  void translateTo(common::cmd::OrderCommand &cmd, int64_t seq,
                   common::api::ApiMoveOrder api) override {
    cmd.command = common::cmd::OrderCommandType::MOVE_ORDER;
    cmd.price = api.newPrice;
    cmd.orderId = api.orderId;
    cmd.symbol = api.symbol;
    cmd.uid = api.uid;
    cmd.timestamp = api.timestamp;
    cmd.resultCode = common::cmd::CommandResultCode::NEW;
  }
};

// Cancel order translator
class CancelOrderTranslator
    : public disruptor::EventTranslatorOneArg<common::cmd::OrderCommand,
                                              common::api::ApiCancelOrder> {
public:
  void translateTo(common::cmd::OrderCommand &cmd, int64_t seq,
                   common::api::ApiCancelOrder api) override {
    cmd.command = common::cmd::OrderCommandType::CANCEL_ORDER;
    cmd.orderId = api.orderId;
    cmd.symbol = api.symbol;
    cmd.uid = api.uid;
    cmd.timestamp = api.timestamp;
    cmd.resultCode = common::cmd::CommandResultCode::NEW;
  }
};

// Reduce order translator
class ReduceOrderTranslator
    : public disruptor::EventTranslatorOneArg<common::cmd::OrderCommand,
                                              common::api::ApiReduceOrder> {
public:
  void translateTo(common::cmd::OrderCommand &cmd, int64_t seq,
                   common::api::ApiReduceOrder api) override {
    cmd.command = common::cmd::OrderCommandType::REDUCE_ORDER;
    cmd.orderId = api.orderId;
    cmd.symbol = api.symbol;
    cmd.uid = api.uid;
    cmd.size = api.reduceSize;
    cmd.timestamp = api.timestamp;
    cmd.resultCode = common::cmd::CommandResultCode::NEW;
  }
};

// Order book request translator
class OrderBookRequestTranslator
    : public disruptor::EventTranslatorOneArg<
          common::cmd::OrderCommand, common::api::ApiOrderBookRequest> {
public:
  void translateTo(common::cmd::OrderCommand &cmd, int64_t seq,
                   common::api::ApiOrderBookRequest api) override {
    cmd.command = common::cmd::OrderCommandType::ORDER_BOOK_REQUEST;
    cmd.symbol = api.symbol;
    cmd.size = api.size;
    cmd.timestamp = api.timestamp;
    cmd.resultCode = common::cmd::CommandResultCode::NEW;
  }
};

// Add user translator
class AddUserTranslator
    : public disruptor::EventTranslatorOneArg<common::cmd::OrderCommand,
                                              common::api::ApiAddUser> {
public:
  void translateTo(common::cmd::OrderCommand &cmd, int64_t seq,
                   common::api::ApiAddUser api) override {
    cmd.command = common::cmd::OrderCommandType::ADD_USER;
    cmd.uid = api.uid;
    cmd.timestamp = api.timestamp;
    cmd.resultCode = common::cmd::CommandResultCode::NEW;
  }
};

// Suspend user translator
class SuspendUserTranslator
    : public disruptor::EventTranslatorOneArg<common::cmd::OrderCommand,
                                              common::api::ApiSuspendUser> {
public:
  void translateTo(common::cmd::OrderCommand &cmd, int64_t seq,
                   common::api::ApiSuspendUser api) override {
    cmd.command = common::cmd::OrderCommandType::SUSPEND_USER;
    cmd.uid = api.uid;
    cmd.timestamp = api.timestamp;
    cmd.resultCode = common::cmd::CommandResultCode::NEW;
  }
};

// Resume user translator
class ResumeUserTranslator
    : public disruptor::EventTranslatorOneArg<common::cmd::OrderCommand,
                                              common::api::ApiResumeUser> {
public:
  void translateTo(common::cmd::OrderCommand &cmd, int64_t seq,
                   common::api::ApiResumeUser api) override {
    cmd.command = common::cmd::OrderCommandType::RESUME_USER;
    cmd.uid = api.uid;
    cmd.timestamp = api.timestamp;
    cmd.resultCode = common::cmd::CommandResultCode::NEW;
  }
};

// Adjust user balance translator
class AdjustUserBalanceTranslator
    : public disruptor::EventTranslatorOneArg<
          common::cmd::OrderCommand, common::api::ApiAdjustUserBalance> {
public:
  void translateTo(common::cmd::OrderCommand &cmd, int64_t seq,
                   common::api::ApiAdjustUserBalance api) override {
    cmd.command = common::cmd::OrderCommandType::BALANCE_ADJUSTMENT;
    cmd.orderId = api.transactionId;
    cmd.symbol = api.currency;
    cmd.uid = api.uid;
    cmd.price = api.amount;
    cmd.orderType = common::OrderTypeFromCode(
        common::BalanceAdjustmentTypeToCode(api.adjustmentType));
    cmd.timestamp = api.timestamp;
    cmd.resultCode = common::cmd::CommandResultCode::NEW;
  }
};

// Reset translator
class ResetTranslator
    : public disruptor::EventTranslatorOneArg<common::cmd::OrderCommand,
                                              common::api::ApiReset> {
public:
  void translateTo(common::cmd::OrderCommand &cmd, int64_t seq,
                   common::api::ApiReset api) override {
    cmd.command = common::cmd::OrderCommandType::RESET;
    cmd.timestamp = api.timestamp;
    cmd.resultCode = common::cmd::CommandResultCode::NEW;
  }
};

// Nop translator
class NopTranslator
    : public disruptor::EventTranslatorOneArg<common::cmd::OrderCommand,
                                              common::api::ApiNop> {
public:
  void translateTo(common::cmd::OrderCommand &cmd, int64_t seq,
                   common::api::ApiNop api) override {
    cmd.command = common::cmd::OrderCommandType::NOP;
    cmd.timestamp = api.timestamp;
    cmd.resultCode = common::cmd::CommandResultCode::NEW;
  }
};

// Static translator instances
static NewOrderTranslator NEW_ORDER_TRANSLATOR;
static MoveOrderTranslator MOVE_ORDER_TRANSLATOR;
static CancelOrderTranslator CANCEL_ORDER_TRANSLATOR;
static ReduceOrderTranslator REDUCE_ORDER_TRANSLATOR;
static OrderBookRequestTranslator ORDER_BOOK_REQUEST_TRANSLATOR;
static AddUserTranslator ADD_USER_TRANSLATOR;
static SuspendUserTranslator SUSPEND_USER_TRANSLATOR;
static ResumeUserTranslator RESUME_USER_TRANSLATOR;
static AdjustUserBalanceTranslator ADJUST_USER_BALANCE_TRANSLATOR;
static ResetTranslator RESET_TRANSLATOR;
static NopTranslator NOP_TRANSLATOR;
} // namespace

template <typename WaitStrategyT>
ExchangeApi<WaitStrategyT>::ExchangeApi(
    disruptor::MultiProducerRingBuffer<common::cmd::OrderCommand, WaitStrategyT>
        *ringBuffer)
    : ringBuffer_(ringBuffer) {}

template <typename WaitStrategyT>
void ExchangeApi<WaitStrategyT>::ProcessResult(int64_t seq,
                                               common::cmd::OrderCommand *cmd) {
  // Check if this is a report query result (BINARY_DATA_QUERY)
  // Match Java: promises.put(seq, orderCommand ->
  // future.complete(translator.apply(orderCommand)))
  typename ReportPromiseMap::accessor reportAccessor;
  if (reportPromises_.find(reportAccessor, seq)) {
    // This is a report query result
    // Extract result from OrderCommand and fulfill promise
    reportAccessor->second(cmd);
    reportPromises_.erase(reportAccessor);
    return;
  }

  // Check if this is an order book request result
  // Match Java: promises.put(seq, cmd1 -> future.complete(cmd1.marketData))
  // Note: For ORDER_BOOK_REQUEST, SimpleEventsProcessor::SendMarketData
  // copies marketData instead of moving it, so cmd->marketData is still available here.
  // We can directly move it to avoid an extra copy.
  typename OrderBookPromiseMap::accessor orderBookAccessor;
  if (orderBookPromises_.find(orderBookAccessor, seq)) {
    // Extract marketData from OrderCommand and fulfill promise
    if (cmd->marketData) {
      // Move marketData directly (SimpleEventsProcessor already copied it for event handler)
      // This avoids an extra copy compared to Copy()
      orderBookAccessor->second.set_value(std::move(cmd->marketData));
    } else {
      // Market data was already moved - this shouldn't happen for ORDER_BOOK_REQUEST
      // since SendMarketData copies instead of moving for this command type
      orderBookAccessor->second.set_value(nullptr);
    }
    orderBookPromises_.erase(orderBookAccessor);
    return;
  }

  // Check for full response promise first (SubmitCommandAsyncFullResponse)
  typename FullResponsePromiseMap::accessor fullResponseAccessor;
  if (fullResponsePromises_.find(fullResponseAccessor, seq)) {
    // Return complete OrderCommand copy
    fullResponseAccessor->second.set_value(cmd->Copy());
    fullResponsePromises_.erase(fullResponseAccessor);
    return;
  }

  // Regular command result (SubmitCommandAsync)
  // TBB concurrent_hash_map: lock-free find and erase
  typename PromiseMap::accessor accessor;
  if (promises_.find(accessor, seq)) {
    accessor->second.set_value(cmd->resultCode);
    promises_.erase(accessor);
  }
  // No promise found - this can happen if:
  // 1. Command was submitted via SubmitCommand (fire-and-forget)
  // 2. Promise was already consumed (shouldn't happen)
  // 3. Sequence mismatch (shouldn't happen)
}

template <typename WaitStrategyT>
void ExchangeApi<WaitStrategyT>::SubmitCommand(common::api::ApiCommand *cmd) {
  if (auto *placeOrder = dynamic_cast<common::api::ApiPlaceOrder *>(cmd)) {
    ringBuffer_->publishEvent(NEW_ORDER_TRANSLATOR, *placeOrder);
  } else if (auto *moveOrder = dynamic_cast<common::api::ApiMoveOrder *>(cmd)) {
    ringBuffer_->publishEvent(MOVE_ORDER_TRANSLATOR, *moveOrder);
  } else if (auto *cancelOrder =
                 dynamic_cast<common::api::ApiCancelOrder *>(cmd)) {
    ringBuffer_->publishEvent(CANCEL_ORDER_TRANSLATOR, *cancelOrder);
  } else if (auto *reduceOrder =
                 dynamic_cast<common::api::ApiReduceOrder *>(cmd)) {
    ringBuffer_->publishEvent(REDUCE_ORDER_TRANSLATOR, *reduceOrder);
  } else if (auto *orderBookRequest =
                 dynamic_cast<common::api::ApiOrderBookRequest *>(cmd)) {
    ringBuffer_->publishEvent(ORDER_BOOK_REQUEST_TRANSLATOR, *orderBookRequest);
  } else if (auto *addUser = dynamic_cast<common::api::ApiAddUser *>(cmd)) {
    ringBuffer_->publishEvent(ADD_USER_TRANSLATOR, *addUser);
  } else if (auto *suspendUser =
                 dynamic_cast<common::api::ApiSuspendUser *>(cmd)) {
    ringBuffer_->publishEvent(SUSPEND_USER_TRANSLATOR, *suspendUser);
  } else if (auto *resumeUser =
                 dynamic_cast<common::api::ApiResumeUser *>(cmd)) {
    ringBuffer_->publishEvent(RESUME_USER_TRANSLATOR, *resumeUser);
  } else if (auto *adjustBalance =
                 dynamic_cast<common::api::ApiAdjustUserBalance *>(cmd)) {
    ringBuffer_->publishEvent(ADJUST_USER_BALANCE_TRANSLATOR, *adjustBalance);
  } else if (auto *reset = dynamic_cast<common::api::ApiReset *>(cmd)) {
    ringBuffer_->publishEvent(RESET_TRANSLATOR, *reset);
  } else if (auto *nop = dynamic_cast<common::api::ApiNop *>(cmd)) {
    ringBuffer_->publishEvent(NOP_TRANSLATOR, *nop);
  } else if (auto *binaryData =
                 dynamic_cast<common::api::ApiBinaryDataCommand *>(cmd)) {
    PublishBinaryData(binaryData, [](int64_t) {});
  } else if (auto *persistState =
                 dynamic_cast<common::api::ApiPersistState *>(cmd)) {
    PublishPersistCmd(persistState, [](int64_t, int64_t) {});
  } else {
    throw std::invalid_argument("Unsupported command type");
  }
}

template <typename WaitStrategyT>
std::future<common::cmd::CommandResultCode>
ExchangeApi<WaitStrategyT>::SubmitCommandAsync(common::api::ApiCommand *cmd) {
  if (!cmd) {
    throw std::invalid_argument("SubmitCommandAsync: cmd is nullptr");
  }
  if (!ringBuffer_) {
    throw std::runtime_error("SubmitCommandAsync: ringBuffer is nullptr");
  }

  std::promise<common::cmd::CommandResultCode> promise;
  auto future = promise.get_future();

  // Handle binary data and persist commands specially - they claim their own
  // sequences
  if (auto *binaryData =
          dynamic_cast<common::api::ApiBinaryDataCommand *>(cmd)) {
    // PublishBinaryData will claim its own sequences and set promises
    // Use shared_ptr to make lambda copyable for std::function
    auto promisePtr =
        std::make_shared<std::promise<common::cmd::CommandResultCode>>(
            std::move(promise));
    PublishBinaryData(binaryData, [this, promisePtr](int64_t seq) {
      // TBB concurrent_hash_map: lock-free insert
      typename PromiseMap::accessor accessor;
      promises_.insert(accessor, seq);
      accessor->second = std::move(*promisePtr);
    });
    return future;
  } else if (auto *persistState =
                 dynamic_cast<common::api::ApiPersistState *>(cmd)) {
    // PublishPersistCmd will claim its own sequences and set promises
    // For persist, we need to wait for both sequences
    std::promise<common::cmd::CommandResultCode> promise2;
    auto future2 = promise2.get_future();
    // Use shared_ptr to make lambda copyable for std::function
    auto promise1Ptr =
        std::make_shared<std::promise<common::cmd::CommandResultCode>>(
            std::move(promise));
    auto promise2Ptr =
        std::make_shared<std::promise<common::cmd::CommandResultCode>>(
            std::move(promise2));
    PublishPersistCmd(persistState, [this, promise1Ptr,
                                     promise2Ptr](int64_t seq1, int64_t seq2) {
      // TBB concurrent_hash_map: lock-free insert
      typename PromiseMap::accessor accessor1, accessor2;
      promises_.insert(accessor1, seq1);
      accessor1->second = std::move(*promise1Ptr);
      promises_.insert(accessor2, seq2);
      accessor2->second = std::move(*promise2Ptr);
    });
    // Return a combined future that waits for both
    // For simplicity, just return the first future (both should complete with
    // same result)
    return future;
  }

  // For other commands, claim sequence and translate
  // Get sequence before publishing
  int64_t seq = ringBuffer_->next();

  // Store promise (TBB concurrent_hash_map: lock-free insert)
  {
    typename PromiseMap::accessor accessor;
    promises_.insert(accessor, seq);
    accessor->second = std::move(promise);
  }

  // Get event slot and translate
  auto &event = ringBuffer_->get(seq);

  // Publish command (manually translate and publish to capture sequence)
  if (auto *placeOrder = dynamic_cast<common::api::ApiPlaceOrder *>(cmd)) {
    NEW_ORDER_TRANSLATOR.translateTo(event, seq, *placeOrder);
  } else if (auto *moveOrder = dynamic_cast<common::api::ApiMoveOrder *>(cmd)) {
    MOVE_ORDER_TRANSLATOR.translateTo(event, seq, *moveOrder);
  } else if (auto *cancelOrder =
                 dynamic_cast<common::api::ApiCancelOrder *>(cmd)) {
    CANCEL_ORDER_TRANSLATOR.translateTo(event, seq, *cancelOrder);
  } else if (auto *reduceOrder =
                 dynamic_cast<common::api::ApiReduceOrder *>(cmd)) {
    REDUCE_ORDER_TRANSLATOR.translateTo(event, seq, *reduceOrder);
  } else if (auto *orderBookRequest =
                 dynamic_cast<common::api::ApiOrderBookRequest *>(cmd)) {
    ORDER_BOOK_REQUEST_TRANSLATOR.translateTo(event, seq, *orderBookRequest);
  } else if (auto *addUser = dynamic_cast<common::api::ApiAddUser *>(cmd)) {
    ADD_USER_TRANSLATOR.translateTo(event, seq, *addUser);
  } else if (auto *suspendUser =
                 dynamic_cast<common::api::ApiSuspendUser *>(cmd)) {
    SUSPEND_USER_TRANSLATOR.translateTo(event, seq, *suspendUser);
  } else if (auto *resumeUser =
                 dynamic_cast<common::api::ApiResumeUser *>(cmd)) {
    RESUME_USER_TRANSLATOR.translateTo(event, seq, *resumeUser);
  } else if (auto *adjustBalance =
                 dynamic_cast<common::api::ApiAdjustUserBalance *>(cmd)) {
    ADJUST_USER_BALANCE_TRANSLATOR.translateTo(event, seq, *adjustBalance);
  } else if (auto *reset = dynamic_cast<common::api::ApiReset *>(cmd)) {
    RESET_TRANSLATOR.translateTo(event, seq, *reset);
  } else if (auto *nop = dynamic_cast<common::api::ApiNop *>(cmd)) {
    NOP_TRANSLATOR.translateTo(event, seq, *nop);
  } else {
    // Remove promise if command type is unsupported
    promises_.erase(seq);
    throw std::invalid_argument("Unsupported command type");
  }

  // Publish the event
  ringBuffer_->publish(seq);

  return future;
}

template <typename WaitStrategyT>
std::future<common::cmd::OrderCommand>
ExchangeApi<WaitStrategyT>::SubmitCommandAsyncFullResponse(
    common::api::ApiCommand *cmd) {
  if (!cmd) {
    throw std::invalid_argument("SubmitCommandAsyncFullResponse: cmd is nullptr");
  }
  if (!ringBuffer_) {
    throw std::runtime_error(
        "SubmitCommandAsyncFullResponse: ringBuffer is nullptr");
  }

  std::promise<common::cmd::OrderCommand> promise;
  auto future = promise.get_future();

  // Handle binary data and persist commands specially - they claim their own
  // sequences
  if (auto *binaryData =
          dynamic_cast<common::api::ApiBinaryDataCommand *>(cmd)) {
    // For binary data commands, we can't use full response (they don't return OrderCommand)
    // Fall back to regular async
    throw std::invalid_argument(
        "SubmitCommandAsyncFullResponse: BinaryDataCommand not supported");
  } else if (auto *persistState =
                 dynamic_cast<common::api::ApiPersistState *>(cmd)) {
    // For persist commands, we can't use full response
    throw std::invalid_argument(
        "SubmitCommandAsyncFullResponse: PersistState not supported");
  }

  // For other commands, claim sequence and translate
  // Get sequence before publishing
  int64_t seq = ringBuffer_->next();

  // Store promise (TBB concurrent_hash_map: lock-free insert)
  {
    typename FullResponsePromiseMap::accessor accessor;
    fullResponsePromises_.insert(accessor, seq);
    accessor->second = std::move(promise);
  }

  // Get event slot and translate
  auto &event = ringBuffer_->get(seq);

  // Publish command (manually translate and publish to capture sequence)
  if (auto *placeOrder = dynamic_cast<common::api::ApiPlaceOrder *>(cmd)) {
    NEW_ORDER_TRANSLATOR.translateTo(event, seq, *placeOrder);
  } else if (auto *moveOrder = dynamic_cast<common::api::ApiMoveOrder *>(cmd)) {
    MOVE_ORDER_TRANSLATOR.translateTo(event, seq, *moveOrder);
  } else if (auto *cancelOrder =
                 dynamic_cast<common::api::ApiCancelOrder *>(cmd)) {
    CANCEL_ORDER_TRANSLATOR.translateTo(event, seq, *cancelOrder);
  } else if (auto *reduceOrder =
                 dynamic_cast<common::api::ApiReduceOrder *>(cmd)) {
    REDUCE_ORDER_TRANSLATOR.translateTo(event, seq, *reduceOrder);
  } else if (auto *orderBookRequest =
                 dynamic_cast<common::api::ApiOrderBookRequest *>(cmd)) {
    ORDER_BOOK_REQUEST_TRANSLATOR.translateTo(event, seq, *orderBookRequest);
  } else if (auto *addUser = dynamic_cast<common::api::ApiAddUser *>(cmd)) {
    ADD_USER_TRANSLATOR.translateTo(event, seq, *addUser);
  } else if (auto *suspendUser =
                 dynamic_cast<common::api::ApiSuspendUser *>(cmd)) {
    SUSPEND_USER_TRANSLATOR.translateTo(event, seq, *suspendUser);
  } else if (auto *resumeUser =
                 dynamic_cast<common::api::ApiResumeUser *>(cmd)) {
    RESUME_USER_TRANSLATOR.translateTo(event, seq, *resumeUser);
  } else if (auto *adjustBalance =
                 dynamic_cast<common::api::ApiAdjustUserBalance *>(cmd)) {
    ADJUST_USER_BALANCE_TRANSLATOR.translateTo(event, seq, *adjustBalance);
  } else if (auto *reset = dynamic_cast<common::api::ApiReset *>(cmd)) {
    RESET_TRANSLATOR.translateTo(event, seq, *reset);
  } else if (auto *nop = dynamic_cast<common::api::ApiNop *>(cmd)) {
    NOP_TRANSLATOR.translateTo(event, seq, *nop);
  } else {
    // Remove promise if command type is unsupported
    fullResponsePromises_.erase(seq);
    throw std::invalid_argument("Unsupported command type");
  }

  // Publish the event
  ringBuffer_->publish(seq);

  return future;
}

template <typename WaitStrategyT>
std::future<std::unique_ptr<common::L2MarketData>>
ExchangeApi<WaitStrategyT>::RequestOrderBookAsync(int32_t symbolId,
                                                  int32_t depth) {
  if (!ringBuffer_) {
    throw std::runtime_error("RequestOrderBookAsync: ringBuffer is nullptr");
  }

  std::promise<std::unique_ptr<common::L2MarketData>> promise;
  auto future = promise.get_future();

  // Get sequence before publishing (match Java: ringBuffer.publishEvent)
  int64_t seq = ringBuffer_->next();

  // Store promise (TBB concurrent_hash_map: lock-free insert)
  {
    typename OrderBookPromiseMap::accessor accessor;
    orderBookPromises_.insert(accessor, seq);
    accessor->second = std::move(promise);
  }

  // Get event slot and set up order book request
  // Match Java: ringBuffer.publishEvent(((cmd, seq) -> { ... promises.put(seq, cmd1 -> future.complete(cmd1.marketData)); }))
  auto &event = ringBuffer_->get(seq);
  event.command = common::cmd::OrderCommandType::ORDER_BOOK_REQUEST;
  event.orderId = -1;
  event.symbol = symbolId;
  event.uid = -1;
  event.size = depth;
  event.timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
                        std::chrono::system_clock::now().time_since_epoch())
                        .count();
  event.resultCode = common::cmd::CommandResultCode::NEW;

  // Publish event
  ringBuffer_->publish(seq);

  return future;
}

template <typename WaitStrategyT>
void ExchangeApi<WaitStrategyT>::SubmitCommandsSync(
    const std::vector<common::api::ApiCommand *> &cmds) {
  if (cmds.empty()) {
    return;
  }

  // Submit all but last
  for (size_t i = 0; i < cmds.size() - 1; i++) {
    SubmitCommand(cmds[i]);
  }

  // Submit last one and wait for result
  auto future = SubmitCommandAsync(cmds[cmds.size() - 1]);
  future.wait(); // Wait for completion
}

template <typename WaitStrategyT>
void ExchangeApi<WaitStrategyT>::PublishCommand(common::api::ApiCommand *cmd,
                                                int64_t seq) {
  SubmitCommand(cmd);
}

template <typename WaitStrategyT>
void ExchangeApi<WaitStrategyT>::PublishBinaryData(
    common::api::ApiBinaryDataCommand *apiCmd,
    std::function<void(int64_t)> endSeqConsumer) {
  if (!apiCmd || !apiCmd->data) {
    throw std::invalid_argument("Invalid ApiBinaryDataCommand");
  }
  if (!ringBuffer_) {
    throw std::runtime_error("PublishBinaryData: ringBuffer is nullptr");
  }

  // Serialize object to bytes
  std::vector<uint8_t> serializedBytes;
  serializedBytes.reserve(128);
  common::VectorBytesOut bytesOut(serializedBytes);
  bytesOut.WriteInt(apiCmd->data->GetBinaryCommandTypeCode());
  apiCmd->data->WriteMarshallable(bytesOut);

  // Compress and convert to long array
  const std::vector<int64_t> longsArrayData =
      utils::SerializationUtils::BytesToLongArrayLz4(serializedBytes,
                                                     LONGS_PER_MESSAGE);

  const int totalNumMessagesToClaim =
      static_cast<int>(longsArrayData.size()) / LONGS_PER_MESSAGE;

  if (totalNumMessagesToClaim == 0) {
    throw std::runtime_error(
        "PublishBinaryData: empty data after serialization");
  }

  // Verify array size is a multiple of LONGS_PER_MESSAGE
  const int expectedArraySize = totalNumMessagesToClaim * LONGS_PER_MESSAGE;
  if (static_cast<int>(longsArrayData.size()) < expectedArraySize) {
    throw std::runtime_error("PublishBinaryData: array size mismatch");
  }

  // Max fragment size is quarter of ring buffer
  const int batchSize = ringBuffer_->getBufferSize() / 4;

  int offset = 0;
  bool isLastFragment = false;
  int fragmentSize = batchSize;

  do {
    if (offset + batchSize >= totalNumMessagesToClaim) {
      fragmentSize = totalNumMessagesToClaim - offset;
      isLastFragment = true;
    }

    // Batch publish: next(n) + publish(lo, hi)
    const int64_t highSeq = ringBuffer_->next(fragmentSize);
    const int64_t lowSeq = highSeq - fragmentSize + 1;

    // Verify sequence range is valid
    if (lowSeq < 0 || highSeq < lowSeq) {
      throw std::runtime_error("PublishBinaryData: invalid sequence range");
    }

    try {
      int ptr = offset * LONGS_PER_MESSAGE;
      // Verify ptr bounds before loop
      const int maxPtr =
          static_cast<int>(longsArrayData.size()) - LONGS_PER_MESSAGE;
      if (ptr < 0 || ptr > maxPtr) {
        throw std::out_of_range("PublishBinaryData: ptr out of range");
      }

      for (int64_t seq = lowSeq; seq <= highSeq; seq++) {
        // Verify bounds before accessing array
        if (ptr + 4 >= static_cast<int>(longsArrayData.size())) {
          throw std::out_of_range(
              "PublishBinaryData: array index out of bounds");
        }

        // Verify ringBuffer is still valid before accessing
        if (!ringBuffer_) {
          throw std::runtime_error(
              "PublishBinaryData: ringBuffer became nullptr");
        }

        auto &cmd = ringBuffer_->get(seq);
        cmd.command = common::cmd::OrderCommandType::BINARY_DATA_COMMAND;
        cmd.userCookie = apiCmd->transferId;
        cmd.symbol = (isLastFragment && seq == highSeq) ? -1 : 0;

        cmd.orderId = longsArrayData[ptr];
        cmd.price = longsArrayData[ptr + 1];
        cmd.reserveBidPrice = longsArrayData[ptr + 2];
        cmd.size = longsArrayData[ptr + 3];
        cmd.uid = longsArrayData[ptr + 4];

        cmd.timestamp = apiCmd->timestamp;
        cmd.resultCode = common::cmd::CommandResultCode::NEW;

        ptr += LONGS_PER_MESSAGE;
      }
    } catch (const std::exception &ex) {
      // Publish even on error to maintain sequence consistency
      ringBuffer_->publish(lowSeq, highSeq);
      throw;
    }

    if (isLastFragment) {
      // Report last sequence before actually publishing data
      endSeqConsumer(highSeq);
    }

    // Batch publish: publish(lo, hi)
    ringBuffer_->publish(lowSeq, highSeq);

    offset += batchSize;
  } while (!isLastFragment);
}

template <typename WaitStrategyT>
void ExchangeApi<WaitStrategyT>::PublishPersistCmd(
    common::api::ApiPersistState *api,
    std::function<void(int64_t, int64_t)> seqConsumer) {
  if (!api) {
    throw std::invalid_argument("Invalid ApiPersistState");
  }

  // Batch publish: next(2) + publish(lo, hi)
  const int64_t secondSeq = ringBuffer_->next(2);
  const int64_t firstSeq = secondSeq - 1;

  try {
    // Will be ignored by risk handlers, but processed by matching engine
    auto &cmdMatching = ringBuffer_->get(firstSeq);
    cmdMatching.command = common::cmd::OrderCommandType::PERSIST_STATE_MATCHING;
    cmdMatching.orderId = api->dumpId;
    cmdMatching.symbol = -1;
    cmdMatching.uid = 0;
    cmdMatching.price = 0;
    cmdMatching.timestamp = api->timestamp;
    cmdMatching.resultCode = common::cmd::CommandResultCode::NEW;

    // Sequential command will make risk handler to create snapshot
    auto &cmdRisk = ringBuffer_->get(secondSeq);
    cmdRisk.command = common::cmd::OrderCommandType::PERSIST_STATE_RISK;
    cmdRisk.orderId = api->dumpId;
    cmdRisk.symbol = -1;
    cmdRisk.uid = 0;
    cmdRisk.price = 0;
    cmdRisk.timestamp = api->timestamp;
    cmdRisk.resultCode = common::cmd::CommandResultCode::NEW;
  } catch (const std::exception &ex) {
    // Publish even on error to maintain sequence consistency
    ringBuffer_->publish(firstSeq, secondSeq);
    throw;
  }

  seqConsumer(firstSeq, secondSeq);
  // Batch publish: publish(lo, hi)
  ringBuffer_->publish(firstSeq, secondSeq);
}

template <typename WaitStrategyT>
void ExchangeApi<WaitStrategyT>::PublishQuery(
    common::api::reports::ApiReportQuery *apiCmd,
    std::function<void(int64_t)> endSeqConsumer) {
  if (!apiCmd || !apiCmd->query) {
    throw std::invalid_argument("Invalid ApiReportQuery");
  }
  if (!ringBuffer_) {
    throw std::runtime_error("PublishQuery: ringBuffer is nullptr");
  }

  // Serialize ReportQuery to bytes (matches Java publishQuery)
  // ReportQuery base class inherits WriteBytesMarshallable (matches Java)
  std::vector<uint8_t> serializedBytes;
  serializedBytes.reserve(128);
  common::VectorBytesOut bytesOut(serializedBytes);
  bytesOut.WriteInt(apiCmd->query->GetReportTypeCode());
  // ReportQuery implements WriteBytesMarshallable (inherited from base class)
  apiCmd->query->WriteMarshallable(bytesOut);

  // Compress and convert to long array
  const std::vector<int64_t> longsArrayData =
      utils::SerializationUtils::BytesToLongArrayLz4(serializedBytes,
                                                     LONGS_PER_MESSAGE);

  const int totalNumMessagesToClaim =
      static_cast<int>(longsArrayData.size()) / LONGS_PER_MESSAGE;

  if (totalNumMessagesToClaim == 0) {
    throw std::runtime_error("PublishQuery: empty data after serialization");
  }

  // Verify array size is a multiple of LONGS_PER_MESSAGE
  const int expectedArraySize = totalNumMessagesToClaim * LONGS_PER_MESSAGE;
  if (static_cast<int>(longsArrayData.size()) < expectedArraySize) {
    throw std::runtime_error("PublishQuery: array size mismatch");
  }

  // Max fragment size is quarter of ring buffer
  const int batchSize = ringBuffer_->getBufferSize() / 4;

  int offset = 0;
  bool isLastFragment = false;
  int fragmentSize = batchSize;

  do {
    if (offset + batchSize >= totalNumMessagesToClaim) {
      fragmentSize = totalNumMessagesToClaim - offset;
      isLastFragment = true;
    }

    // Batch publish: next(n) + publish(lo, hi)
    const int64_t highSeq = ringBuffer_->next(fragmentSize);
    const int64_t lowSeq = highSeq - fragmentSize + 1;

    // Verify sequence range is valid
    if (lowSeq < 0 || highSeq < lowSeq) {
      throw std::runtime_error("PublishQuery: invalid sequence range");
    }

    try {
      int ptr = offset * LONGS_PER_MESSAGE;
      // Verify ptr bounds before loop
      const int maxPtr =
          static_cast<int>(longsArrayData.size()) - LONGS_PER_MESSAGE;
      if (ptr < 0 || ptr > maxPtr) {
        throw std::out_of_range("PublishQuery: ptr out of range");
      }

      for (int64_t seq = lowSeq; seq <= highSeq; seq++) {
        // Verify bounds before accessing array
        if (ptr + 4 >= static_cast<int>(longsArrayData.size())) {
          throw std::out_of_range("PublishQuery: array index out of bounds");
        }

        // Verify ringBuffer is still valid before accessing
        if (!ringBuffer_) {
          throw std::runtime_error("PublishQuery: ringBuffer became nullptr");
        }

        auto &cmd = ringBuffer_->get(seq);
        // Use BINARY_DATA_QUERY instead of BINARY_DATA_COMMAND
        cmd.command = common::cmd::OrderCommandType::BINARY_DATA_QUERY;
        cmd.userCookie = apiCmd->transferId;
        cmd.symbol = (isLastFragment && seq == highSeq) ? -1 : 0;

        cmd.orderId = longsArrayData[ptr];
        cmd.price = longsArrayData[ptr + 1];
        cmd.reserveBidPrice = longsArrayData[ptr + 2];
        cmd.size = longsArrayData[ptr + 3];
        cmd.uid = longsArrayData[ptr + 4];

        cmd.timestamp = apiCmd->timestamp;
        cmd.resultCode = common::cmd::CommandResultCode::NEW;

        ptr += LONGS_PER_MESSAGE;
      }
    } catch (const std::exception &ex) {
      // Publish even on error to maintain sequence consistency
      ringBuffer_->publish(lowSeq, highSeq);
      throw;
    }

    if (isLastFragment) {
      // Report last sequence before actually publishing data
      endSeqConsumer(highSeq);
    }

    // Batch publish: publish(lo, hi)
    ringBuffer_->publish(lowSeq, highSeq);

    offset += batchSize;
  } while (!isLastFragment);
}

template <typename WaitStrategyT>
template <typename Q, typename R>
std::future<std::unique_ptr<R>>
ExchangeApi<WaitStrategyT>::ProcessReport(std::unique_ptr<Q> query,
                                          int32_t transferId) {
  if (!query) {
    throw std::invalid_argument("ProcessReport: query is nullptr");
  }
  if (!ringBuffer_) {
    throw std::runtime_error("ProcessReport: ringBuffer is nullptr");
  }

  // Create promise for result
  std::promise<std::unique_ptr<R>> promise;
  auto future = promise.get_future();

  // Wrap query in ApiReportQuery
  auto apiReportQuery = std::make_unique<common::api::reports::ApiReportQuery>(
      transferId, std::move(query));

  // Use shared_ptr to make lambda copyable for std::function
  auto promisePtr =
      std::make_shared<std::promise<std::unique_ptr<R>>>(std::move(promise));
  auto queryPtr = apiReportQuery->query.get();

  // Publish query and set up callback to extract result from OrderCommand
  // Match Java: submitQueryAsync with translator function
  PublishQuery(apiReportQuery.get(), [this, promisePtr, queryPtr](int64_t seq) {
    // Store translator function that will extract result from OrderCommand
    // Match Java: cmd ->
    // query.createResult(OrderBookEventsHelper.deserializeEvents(cmd).values().parallelStream().map(Wire::bytes))
    typename ReportPromiseMap::accessor accessor;
    reportPromises_.insert(accessor, seq);
    accessor->second = [promisePtr, queryPtr](common::cmd::OrderCommand *cmd) {
      // Extract binary events from OrderCommand
      auto sectionsMap =
          orderbook::OrderBookEventsHelper::DeserializeEvents(cmd);

      // Convert map values to vector of BytesIn pointers
      // Match Java: .map(Wire::bytes)
      // Skip empty wires (matches Java behavior where empty sections are filtered)
      std::vector<common::BytesIn *> sections;
      sections.reserve(sectionsMap.size());
      std::vector<common::Wire> wireOwners;
      wireOwners.reserve(sectionsMap.size());

      for (const auto &[sectionId, wire] : sectionsMap) {
        const auto &bytes = wire.GetBytes();
        if (!bytes.empty()) {
          wireOwners.push_back(wire);
          sections.push_back(&wireOwners.back().bytes());
        }
      }

      // Call query.createResult() to merge sections
      auto result = queryPtr->CreateResult(sections);

      // Set promise value
      promisePtr->set_value(
          std::unique_ptr<R>(static_cast<R *>(result.release())));
    };
  });

  return future;
}

template <typename WaitStrategyT>
std::future<std::vector<std::vector<uint8_t>>>
ExchangeApi<WaitStrategyT>::ProcessReportAny(int32_t queryTypeId,
                                             std::vector<uint8_t> queryBytes,
                                             int32_t transferId) {
  if (!ringBuffer_) {
    throw std::runtime_error("ProcessReportAny: ringBuffer is nullptr");
  }

  // Deserialize query from bytes using factory
  common::VectorBytesIn bytesIn(queryBytes);
  // Skip type code (already known from queryTypeId)
  bytesIn.ReadInt(); // Skip the type code in bytes

  common::api::reports::ReportType reportType =
      common::api::reports::ReportTypeFromCode(queryTypeId);

  // Create query using factory (type-erased)
  void *queryPtr =
      common::api::reports::ReportQueryFactory::getInstance().createQuery(
          reportType, bytesIn);

  if (!queryPtr) {
    throw std::runtime_error(
        "ProcessReportAny: Failed to create query from bytes");
  }

  // Create promise for sections result
  std::promise<std::vector<std::vector<uint8_t>>> promise;
  auto future = promise.get_future();

  // Wrap query in ApiReportQuery (using type-erased pointer)
  // We need to store the query pointer and type info
  auto promisePtr =
      std::make_shared<std::promise<std::vector<std::vector<uint8_t>>>>(
          std::move(promise));

  // Publish query - we'll handle the result extraction in ProcessResult
  // For now, we need to serialize the query again to publish it
  // Actually, we already have the serialized bytes, so we can use them directly
  std::vector<uint8_t> serializedBytes = std::move(queryBytes);

  // Compress and convert to long array
  const std::vector<int64_t> longsArrayData =
      utils::SerializationUtils::BytesToLongArrayLz4(serializedBytes,
                                                     LONGS_PER_MESSAGE);

  const int totalNumMessagesToClaim =
      static_cast<int>(longsArrayData.size()) / LONGS_PER_MESSAGE;

  if (totalNumMessagesToClaim == 0) {
    throw std::runtime_error(
        "ProcessReportAny: empty data after serialization");
  }

  // Max fragment size is quarter of ring buffer
  const int batchSize = ringBuffer_->getBufferSize() / 4;

  int offset = 0;
  bool isLastFragment = false;
  int fragmentSize = batchSize;

  do {
    if (offset + batchSize >= totalNumMessagesToClaim) {
      fragmentSize = totalNumMessagesToClaim - offset;
      isLastFragment = true;
    }

    // Batch publish: next(n) + publish(lo, hi)
    const int64_t highSeq = ringBuffer_->next(fragmentSize);
    const int64_t lowSeq = highSeq - fragmentSize + 1;

    try {
      int ptr = offset * LONGS_PER_MESSAGE;
      const int maxPtr =
          static_cast<int>(longsArrayData.size()) - LONGS_PER_MESSAGE;
      if (ptr < 0 || ptr > maxPtr) {
        throw std::out_of_range("ProcessReportAny: ptr out of range");
      }

      for (int64_t seq = lowSeq; seq <= highSeq; seq++) {
        if (ptr + 4 >= static_cast<int>(longsArrayData.size())) {
          throw std::out_of_range(
              "ProcessReportAny: array index out of bounds");
        }

        auto &cmd = ringBuffer_->get(seq);
        cmd.command = common::cmd::OrderCommandType::BINARY_DATA_QUERY;
        cmd.userCookie = transferId;
        cmd.symbol = (isLastFragment && seq == highSeq) ? -1 : 0;

        cmd.orderId = longsArrayData[ptr];
        cmd.price = longsArrayData[ptr + 1];
        cmd.reserveBidPrice = longsArrayData[ptr + 2];
        cmd.size = longsArrayData[ptr + 3];
        cmd.uid = longsArrayData[ptr + 4];

        cmd.resultCode = common::cmd::CommandResultCode::NEW;

        ptr += LONGS_PER_MESSAGE;
      }
    } catch (const std::exception &ex) {
      ringBuffer_->publish(lowSeq, highSeq);
      throw;
    }

    if (isLastFragment) {
      // Store promise for result extraction
      typename ReportPromiseMap::accessor accessor;
      reportPromises_.insert(accessor, highSeq);
      accessor->second = [promisePtr, queryPtr,
                          reportType](common::cmd::OrderCommand *cmd) {
        // Extract binary events from OrderCommand
        auto sectionsMap =
            orderbook::OrderBookEventsHelper::DeserializeEvents(cmd);

        // Convert map values to vector of byte vectors
        // Match Java: .map(Wire::bytes) but for ProcessReportAny we need bytes
        std::vector<std::vector<uint8_t>> sections;
        sections.reserve(sectionsMap.size());
        for (const auto &[sectionId, wire] : sectionsMap) {
          const auto &bytes = wire.GetBytes();
          if (!bytes.empty()) {
            sections.push_back(bytes);
          }
        }

        // Clean up query pointer (was created with new)
        // We need to know the type to delete it properly
        // For now, we'll leak it (not ideal, but type erasure makes this hard)
        // TODO: Store deleter function in factory

        promisePtr->set_value(std::move(sections));
      };
    }

    // Batch publish: publish(lo, hi)
    ringBuffer_->publish(lowSeq, highSeq);

    offset += batchSize;
  } while (!isLastFragment);

  // Clean up query pointer - we need to delete it properly
  // For now, we'll store it and delete it after the result is received
  // Actually, we can't easily do this with type erasure...
  // Let's use a shared_ptr with custom deleter, but that requires storing the
  // type For now, we'll just leak it (not ideal, but works)
  // TODO: Improve this by storing deleter in factory

  return future;
}

} // namespace core
} // namespace exchange

// Explicit template instantiations for common wait strategies
// Note: These must be outside the namespace
#include <disruptor/BlockingWaitStrategy.h>
#include <disruptor/BusySpinWaitStrategy.h>
#include <disruptor/YieldingWaitStrategy.h>

template class exchange::core::ExchangeApi<disruptor::BlockingWaitStrategy>;
template class exchange::core::ExchangeApi<disruptor::YieldingWaitStrategy>;
template class exchange::core::ExchangeApi<disruptor::BusySpinWaitStrategy>;
