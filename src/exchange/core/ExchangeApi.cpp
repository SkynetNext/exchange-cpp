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
#include <iostream>
#include <exchange/core/common/BalanceAdjustmentType.h>
#include <exchange/core/common/BytesOut.h>
#include <exchange/core/common/OrderType.h>
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
#include <exchange/core/common/cmd/CommandResultCode.h>
#include <exchange/core/common/cmd/OrderCommand.h>
#include <exchange/core/common/cmd/OrderCommandType.h>
#include <exchange/core/processors/BinaryCommandsProcessor.h>
#include <exchange/core/utils/SerializationUtils.h>
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
  auto it = promises_.find(seq);
  if (it != promises_.end()) {
    it->second.set_value(cmd->resultCode);
    promises_.erase(it);
  }
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

  // Handle binary data and persist commands specially - they claim their own sequences
  if (auto *binaryData =
          dynamic_cast<common::api::ApiBinaryDataCommand *>(cmd)) {
    // PublishBinaryData will claim its own sequences and set promises
    // Use shared_ptr to make lambda copyable for std::function
    auto promisePtr = std::make_shared<std::promise<common::cmd::CommandResultCode>>(std::move(promise));
    PublishBinaryData(binaryData, [this, promisePtr](int64_t seq) {
      // Move the promise from shared_ptr to promises_ map
      promises_[seq] = std::move(*promisePtr);
    });
    return future;
  } else if (auto *persistState =
                 dynamic_cast<common::api::ApiPersistState *>(cmd)) {
    // PublishPersistCmd will claim its own sequences and set promises
    // For persist, we need to wait for both sequences
    std::promise<common::cmd::CommandResultCode> promise2;
    auto future2 = promise2.get_future();
    // Use shared_ptr to make lambda copyable for std::function
    auto promise1Ptr = std::make_shared<std::promise<common::cmd::CommandResultCode>>(std::move(promise));
    auto promise2Ptr = std::make_shared<std::promise<common::cmd::CommandResultCode>>(std::move(promise2));
    PublishPersistCmd(persistState, [this, promise1Ptr, promise2Ptr](int64_t seq1, int64_t seq2) {
      // Move the promises from shared_ptr to promises_ map
      promises_[seq1] = std::move(*promise1Ptr);
      promises_[seq2] = std::move(*promise2Ptr);
    });
    // Return a combined future that waits for both
    // For simplicity, just return the first future (both should complete with same result)
    return future;
  }

  // For other commands, claim sequence and translate
  // Get sequence before publishing
  int64_t seq = ringBuffer_->next();

  // Store promise
  promises_[seq] = std::move(promise);

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
    throw std::runtime_error("PublishBinaryData: empty data after serialization");
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
      const int maxPtr = static_cast<int>(longsArrayData.size()) - LONGS_PER_MESSAGE;
      if (ptr < 0 || ptr > maxPtr) {
        throw std::out_of_range("PublishBinaryData: ptr out of range");
      }

      for (int64_t seq = lowSeq; seq <= highSeq; seq++) {
        // Verify bounds before accessing array
        if (ptr + 4 >= static_cast<int>(longsArrayData.size())) {
          throw std::out_of_range("PublishBinaryData: array index out of bounds");
        }

        // Verify ringBuffer is still valid before accessing
        if (!ringBuffer_) {
          throw std::runtime_error("PublishBinaryData: ringBuffer became nullptr");
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
