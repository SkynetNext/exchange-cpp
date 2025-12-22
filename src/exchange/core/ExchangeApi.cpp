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
#include <exchange/core/common/OrderType.h>
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
#include <stdexcept>

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
    it->second(cmd, seq);
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
  } else {
    throw std::invalid_argument("Unsupported command type");
  }
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

  // Submit last one (simplified - would use future in real impl)
  SubmitCommand(cmds[cmds.size() - 1]);
}

template <typename WaitStrategyT>
void ExchangeApi<WaitStrategyT>::PublishCommand(common::api::ApiCommand *cmd,
                                                int64_t seq) {
  // This method is no longer needed - SubmitCommand handles everything
  // Keeping for backward compatibility
  SubmitCommand(cmd);
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
