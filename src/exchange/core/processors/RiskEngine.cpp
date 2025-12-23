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

#include <exchange/core/collections/objpool/ObjectsPool.h>
#include <exchange/core/common/BalanceAdjustmentType.h>
#include <exchange/core/common/BytesIn.h>
#include <exchange/core/common/MatcherEventType.h>
#include <exchange/core/common/OrderAction.h>
#include <exchange/core/common/OrderType.h>
#include <exchange/core/common/SymbolType.h>
#include <exchange/core/common/api/binary/BatchAddAccountsCommand.h>
#include <exchange/core/common/api/binary/BatchAddSymbolsCommand.h>
#include <exchange/core/common/api/reports/ReportResult.h>
#include <exchange/core/common/cmd/CommandResultCode.h>
#include <exchange/core/common/cmd/OrderCommand.h>
#include <exchange/core/common/cmd/OrderCommandType.h>
#include <exchange/core/common/config/ExchangeConfiguration.h>
#include <exchange/core/common/config/OrdersProcessingConfiguration.h>
#include <exchange/core/processors/BinaryCommandsProcessor.h>
#include <exchange/core/processors/RiskEngine.h>
#include <exchange/core/processors/RiskEngineReportQueriesHandler.h>
#include <exchange/core/processors/UserProfileService.h>
#include <exchange/core/processors/journaling/ISerializationProcessor.h>
#include <exchange/core/utils/CoreArithmeticUtils.h>
#include <exchange/core/utils/SerializationUtils.h>
#include <exchange/core/utils/UnsafeUtils.h>
#include <stdexcept>
#include <unordered_map>

namespace exchange {
namespace core {
namespace processors {

RiskEngine::RiskEngine(
    int32_t shardId, int64_t numShards,
    journaling::ISerializationProcessor *serializationProcessor,
    SharedPool *sharedPool,
    const common::config::ExchangeConfiguration *exchangeConfiguration)
    : shardId_(shardId), shardMask_(numShards - 1), exchangeId_(""),
      folder_(""), cfgIgnoreRiskProcessing_(false),
      cfgMarginTradingEnabled_(false), logDebug_(false) {
  // Validate numShards is power of 2
  if ((numShards & (numShards - 1)) != 0) {
    throw std::invalid_argument("Invalid number of shards " +
                                std::to_string(numShards) +
                                " - must be power of 2");
  }

  const auto *initStateCfg = &exchangeConfiguration->initStateCfg;
  const auto *ordersProcCfg = &exchangeConfiguration->ordersProcessingCfg;
  const auto *reportsQueriesCfg = &exchangeConfiguration->reportsQueriesCfg;

  // Initialize exchangeId and folder
  exchangeId_ = initStateCfg->exchangeId;
  // TODO: Use DiskSerializationProcessorConfiguration::DEFAULT_FOLDER
  folder_ = std::filesystem::path("snapshots");

  // Initialize configuration flags
  cfgIgnoreRiskProcessing_ = (ordersProcCfg->riskProcessingMode ==
                              common::config::OrdersProcessingConfiguration::
                                  RiskProcessingMode::NO_RISK_PROCESSING);
  cfgMarginTradingEnabled_ = (ordersProcCfg->marginTradingMode ==
                              common::config::OrdersProcessingConfiguration::
                                  MarginTradingMode::MARGIN_TRADING_ENABLED);

  // Initialize logging configuration
  const auto *loggingCfg = &exchangeConfiguration->loggingCfg;
  logDebug_ = loggingCfg->Contains(
      common::config::LoggingConfiguration::LoggingLevel::LOGGING_RISK_DEBUG);

  // Initialize object pools
  // Matches Java RiskEngine configuration (SYMBOL_POSITION_RECORD = 1024 * 256)
  // TODO: Move to performance configuration
  std::unordered_map<int, int> objectsPoolConfig;
  objectsPoolConfig[::exchange::core::collections::objpool::ObjectsPool::
                        SYMBOL_POSITION_RECORD] = 1024 * 256;
  objectsPool_ =
      std::unique_ptr<::exchange::core::collections::objpool::ObjectsPool>(
          new ::exchange::core::collections::objpool::ObjectsPool(
              objectsPoolConfig));

  // Try to load from snapshot
  if (journaling::ISerializationProcessor::CanLoadFromSnapshot(
          serializationProcessor, initStateCfg, shardId_,
          journaling::ISerializationProcessor::SerializedModuleType::
              RISK_ENGINE)) {
    // Load from snapshot
    auto state = serializationProcessor->LoadData<void *>(
        initStateCfg->snapshotId,
        journaling::ISerializationProcessor::SerializedModuleType::RISK_ENGINE,
        shardId_,
        [this, sharedPool,
         reportsQueriesCfg](common::BytesIn *bytesIn) -> void * {
          if (bytesIn == nullptr) {
            throw std::invalid_argument("BytesIn cannot be nullptr");
          }
          // Verify shardId and shardMask
          if (shardId_ != bytesIn->ReadInt()) {
            throw std::runtime_error("wrong shardId");
          }
          if (shardMask_ != bytesIn->ReadLong()) {
            throw std::runtime_error("wrong shardMask");
          }

          // Deserialize SymbolSpecificationProvider
          symbolSpecificationProvider_ =
              std::make_unique<SymbolSpecificationProvider>(bytesIn);

          // Deserialize UserProfileService
          userProfileService_ = std::make_unique<UserProfileService>(bytesIn);

          // Create ReportQueriesHandler adapter to forward queries to
          // RiskEngine
          reportQueriesHandler_ =
              std::make_unique<RiskEngineReportQueriesHandler>(this);

          // Deserialize BinaryCommandsProcessor
          binaryCommandsProcessor_ = std::make_unique<BinaryCommandsProcessor>(
              [this](common::api::binary::BinaryDataCommand *msg) {
                HandleBinaryMessage(msg);
              },
              reportQueriesHandler_.get(), sharedPool, reportsQueriesCfg,
              bytesIn, shardId_);

          // Deserialize lastPriceCache (int -> LastPriceCacheRecord)
          int lastPriceCacheLength = bytesIn->ReadInt();
          for (int i = 0; i < lastPriceCacheLength; i++) {
            int32_t symbolId = bytesIn->ReadInt();
            LastPriceCacheRecord record(*bytesIn);
            lastPriceCache_[symbolId] = record;
          }

          // Deserialize fees (int -> long)
          fees_ = utils::SerializationUtils::ReadIntLongHashMap(*bytesIn);

          // Deserialize adjustments (int -> long)
          adjustments_ =
              utils::SerializationUtils::ReadIntLongHashMap(*bytesIn);

          // Deserialize suspends (int -> long)
          suspends_ = utils::SerializationUtils::ReadIntLongHashMap(*bytesIn);

          return nullptr; // Not used
        });
  } else {
    // Initialize services normally
    symbolSpecificationProvider_ =
        std::make_unique<SymbolSpecificationProvider>();
    userProfileService_ = std::make_unique<UserProfileService>();

    // Create ReportQueriesHandler adapter to forward queries to RiskEngine
    reportQueriesHandler_ =
        std::make_unique<RiskEngineReportQueriesHandler>(this);

    binaryCommandsProcessor_ = std::make_unique<BinaryCommandsProcessor>(
        [this](common::api::binary::BinaryDataCommand *msg) {
          HandleBinaryMessage(msg);
        },
        reportQueriesHandler_.get(), sharedPool, reportsQueriesCfg, shardId_);
  }
}

bool RiskEngine::PreProcessCommand(int64_t seq,
                                   common::cmd::OrderCommand *cmd) {
  switch (cmd->command) {
  case common::cmd::OrderCommandType::MOVE_ORDER:
  case common::cmd::OrderCommandType::CANCEL_ORDER:
  case common::cmd::OrderCommandType::REDUCE_ORDER:
  case common::cmd::OrderCommandType::ORDER_BOOK_REQUEST:
    return false;

  case common::cmd::OrderCommandType::PLACE_ORDER:
    if (UidForThisHandler(cmd->uid)) {
      cmd->resultCode = PlaceOrderRiskCheck(cmd);
    }
    return false;

  case common::cmd::OrderCommandType::ADD_USER:
    if (UidForThisHandler(cmd->uid)) {
      cmd->resultCode = userProfileService_->AddUser(cmd->uid);
    }
    return false;

  case common::cmd::OrderCommandType::BALANCE_ADJUSTMENT:
    if (UidForThisHandler(cmd->uid)) {
      auto adjustmentType = common::BalanceAdjustmentTypeFromCode(
          static_cast<int8_t>(cmd->orderType));
      cmd->resultCode = AdjustBalance(cmd->uid, cmd->symbol, cmd->price,
                                      cmd->orderId, adjustmentType);
    }
    return false;

  case common::cmd::OrderCommandType::SUSPEND_USER:
    if (UidForThisHandler(cmd->uid)) {
      cmd->resultCode = userProfileService_->SuspendUser(cmd->uid);
    }
    return false;

  case common::cmd::OrderCommandType::RESUME_USER:
    if (UidForThisHandler(cmd->uid)) {
      cmd->resultCode = userProfileService_->ResumeUser(cmd->uid);
    }
    return false;

  case common::cmd::OrderCommandType::BINARY_DATA_COMMAND:
  case common::cmd::OrderCommandType::BINARY_DATA_QUERY:
    binaryCommandsProcessor_->AcceptBinaryFrame(cmd);
    if (shardId_ == 0) {
      cmd->resultCode =
          common::cmd::CommandResultCode::VALID_FOR_MATCHING_ENGINE;
    }
    return false;

  case common::cmd::OrderCommandType::RESET:
    Reset();
    if (shardId_ == 0) {
      cmd->resultCode = common::cmd::CommandResultCode::SUCCESS;
    }
    return false;

  case common::cmd::OrderCommandType::PERSIST_STATE_MATCHING:
    if (shardId_ == 0) {
      cmd->resultCode =
          common::cmd::CommandResultCode::VALID_FOR_MATCHING_ENGINE;
    }
    return true; // true = publish sequence before finishing processing whole
                 // batch

  case common::cmd::OrderCommandType::PERSIST_STATE_RISK:
    // TODO: Implement serialization
    cmd->resultCode = common::cmd::CommandResultCode::SUCCESS;
    return false;

  default:
    return false;
  }
}

void RiskEngine::PostProcessCommand(int64_t seq,
                                    common::cmd::OrderCommand *cmd) {
  const int32_t symbol = cmd->symbol;
  const auto *marketData = cmd->marketData.get();
  common::MatcherTradeEvent *mte = cmd->matcherEvent;

  // skip events processing if no events (or if contains BINARY EVENT)
  if (marketData == nullptr &&
      (mte == nullptr ||
       mte->eventType == common::MatcherEventType::BINARY_EVENT)) {
    return;
  }

  const auto *spec =
      symbolSpecificationProvider_->GetSymbolSpecification(symbol);
  if (spec == nullptr) {
    throw std::runtime_error("Symbol not found: " + std::to_string(symbol));
  }

  const bool takerSell = (cmd->action == common::OrderAction::ASK);

  if (mte != nullptr &&
      mte->eventType != common::MatcherEventType::BINARY_EVENT) {
    // at least one event to process
    if (spec->type == common::SymbolType::CURRENCY_EXCHANGE_PAIR) {
      // Handle exchange pair trades
      auto *takerUp =
          UidForThisHandler(cmd->uid)
              ? userProfileService_->GetUserProfileOrAddSuspended(cmd->uid)
              : nullptr;

      // REJECT always comes first; REDUCE is always single event
      if (mte->eventType == common::MatcherEventType::REDUCE ||
          mte->eventType == common::MatcherEventType::REJECT) {
        if (takerUp != nullptr) {
          HandleMatcherRejectReduceEventExchange(cmd, mte, spec, takerSell,
                                                 takerUp);
        }
        mte = mte->nextEvent;
      }

      if (mte != nullptr) {
        if (takerSell) {
          HandleMatcherEventsExchangeSell(mte, spec, takerUp);
        } else {
          HandleMatcherEventsExchangeBuy(mte, spec, takerUp, cmd);
        }
      }
    } else if (spec->type == common::SymbolType::FUTURES_CONTRACT) {
      // Handle margin trades
      auto *takerUp =
          UidForThisHandler(cmd->uid)
              ? userProfileService_->GetUserProfileOrAddSuspended(cmd->uid)
              : nullptr;

      // for margin-mode symbols also resolve position record
      common::SymbolPositionRecord *takerSpr = nullptr;
      if (takerUp != nullptr) {
        takerSpr = takerUp->GetPositionRecordOrThrowEx(symbol);
      }

      do {
        HandleMatcherEventMargin(mte, spec, cmd->action, takerUp, takerSpr);
        mte = mte->nextEvent;
      } while (mte != nullptr);
    }
  }

  // Process market data
  if (marketData != nullptr && cfgMarginTradingEnabled_) {
    auto &record = lastPriceCache_[symbol];
    record.askPrice =
        (marketData->askSize != 0) ? marketData->askPrices[0] : INT64_MAX;
    record.bidPrice = (marketData->bidSize != 0) ? marketData->bidPrices[0] : 0;
  }
}

bool RiskEngine::UidForThisHandler(int64_t uid) const {
  return (shardMask_ == 0) || ((uid & shardMask_) == shardId_);
}

void RiskEngine::Reset() {
  symbolSpecificationProvider_->Reset();
  userProfileService_->Reset();
  binaryCommandsProcessor_->Reset();
  lastPriceCache_.clear();
  fees_.clear();
  adjustments_.clear();
  suspends_.clear();
}

common::cmd::CommandResultCode
RiskEngine::PlaceOrderRiskCheck(common::cmd::OrderCommand *cmd) {
  auto *userProfile = userProfileService_->GetUserProfile(cmd->uid);
  if (userProfile == nullptr) {
    return common::cmd::CommandResultCode::AUTH_INVALID_USER;
  }

  const auto *spec =
      symbolSpecificationProvider_->GetSymbolSpecification(cmd->symbol);
  if (spec == nullptr) {
    return common::cmd::CommandResultCode::INVALID_SYMBOL;
  }

  if (cfgIgnoreRiskProcessing_) {
    // skip processing
    return common::cmd::CommandResultCode::VALID_FOR_MATCHING_ENGINE;
  }

  // check if account has enough funds
  const auto resultCode = PlaceOrder(cmd, userProfile, spec);

  if (resultCode != common::cmd::CommandResultCode::VALID_FOR_MATCHING_ENGINE) {
    return common::cmd::CommandResultCode::RISK_NSF;
  }

  return resultCode;
}

void RiskEngine::HandleBinaryMessage(
    common::api::binary::BinaryDataCommand *message) {
  if (auto *batchAddSymbols =
          dynamic_cast<common::api::binary::BatchAddSymbolsCommand *>(
              message)) {
    // Handle batch add symbols
    const auto &symbols = batchAddSymbols->symbols;
    for (const auto &[symbolId, spec] : symbols) {
      if (spec != nullptr &&
          (spec->type == common::SymbolType::CURRENCY_EXCHANGE_PAIR ||
           cfgMarginTradingEnabled_)) {
        symbolSpecificationProvider_->AddSymbol(spec);
      }
    }
  } else if (auto *batchAddAccounts =
                 dynamic_cast<common::api::binary::BatchAddAccountsCommand *>(
                     message)) {
    // Handle batch add accounts
    const auto &users = batchAddAccounts->users;
    for (const auto &[uid, accounts] : users) {
      if (userProfileService_->AddUser(uid) ==
          common::cmd::CommandResultCode::SUCCESS) {
        for (const auto &[cur, bal] : accounts) {
          AdjustBalance(uid, cur, bal, 1'000'000'000 + cur,
                        common::BalanceAdjustmentType::ADJUSTMENT);
        }
      }
    }
  }
}

template <typename R>
std::optional<std::unique_ptr<R>> RiskEngine::HandleReportQuery(
    common::api::reports::ReportQuery<R> *reportQuery) {
  return reportQuery->Process(this);
}

// Private helper methods (simplified implementations)
common::cmd::CommandResultCode
RiskEngine::PlaceOrder(common::cmd::OrderCommand *cmd,
                       common::UserProfile *userProfile,
                       const common::CoreSymbolSpecification *spec) {
  if (spec->type == common::SymbolType::CURRENCY_EXCHANGE_PAIR) {
    return PlaceExchangeOrder(cmd, userProfile, spec);
  } else if (spec->type == common::SymbolType::FUTURES_CONTRACT) {
    if (!cfgMarginTradingEnabled_) {
      return common::cmd::CommandResultCode::RISK_MARGIN_TRADING_DISABLED;
    }
    // TODO: Implement margin order placement
    return common::cmd::CommandResultCode::VALID_FOR_MATCHING_ENGINE;
  } else {
    return common::cmd::CommandResultCode::UNSUPPORTED_SYMBOL_TYPE;
  }
}

common::cmd::CommandResultCode
RiskEngine::PlaceExchangeOrder(common::cmd::OrderCommand *cmd,
                               common::UserProfile *userProfile,
                               const common::CoreSymbolSpecification *spec) {
  const int32_t currency = (cmd->action == common::OrderAction::BID)
                               ? spec->quoteCurrency
                               : spec->baseCurrency;

  // Calculate order hold amount
  int64_t orderHoldAmount;
  if (cmd->action == common::OrderAction::BID) {
    if (cmd->orderType == common::OrderType::FOK_BUDGET ||
        cmd->orderType == common::OrderType::IOC_BUDGET) {
      if (cmd->reserveBidPrice != cmd->price) {
        return common::cmd::CommandResultCode::RISK_INVALID_RESERVE_BID_PRICE;
      }
      orderHoldAmount =
          utils::CoreArithmeticUtils::CalculateAmountBidTakerFeeForBudget(
              cmd->size, cmd->price, spec);
    } else {
      if (cmd->reserveBidPrice < cmd->price) {
        return common::cmd::CommandResultCode::RISK_INVALID_RESERVE_BID_PRICE;
      }
      orderHoldAmount = utils::CoreArithmeticUtils::CalculateAmountBidTakerFee(
          cmd->size, cmd->reserveBidPrice, spec);
    }
  } else {
    if (cmd->price * spec->quoteScaleK < spec->takerFee) {
      return common::cmd::CommandResultCode::RISK_ASK_PRICE_LOWER_THAN_FEE;
    }
    orderHoldAmount =
        utils::CoreArithmeticUtils::CalculateAmountAsk(cmd->size, spec);
  }

  // Check balance
  int64_t currentBalance = userProfile->accounts[currency];
  if (currentBalance < orderHoldAmount) {
    return common::cmd::CommandResultCode::RISK_NSF;
  }

  // Hold the amount (speculative change)
  userProfile->accounts[currency] = currentBalance - orderHoldAmount;

  return common::cmd::CommandResultCode::VALID_FOR_MATCHING_ENGINE;
}

common::cmd::CommandResultCode
RiskEngine::AdjustBalance(int64_t uid, int32_t currency, int64_t amountDiff,
                          int64_t fundingTransactionId,
                          common::BalanceAdjustmentType adjustmentType) {
  const auto res = userProfileService_->BalanceAdjustment(
      uid, currency, amountDiff, fundingTransactionId);
  if (res == common::cmd::CommandResultCode::SUCCESS) {
    switch (adjustmentType) {
    case common::BalanceAdjustmentType::ADJUSTMENT:
      adjustments_[currency] -= amountDiff;
      break;
    case common::BalanceAdjustmentType::SUSPEND:
      suspends_[currency] -= amountDiff;
      break;
    }
  }
  return res;
}

void RiskEngine::HandleMatcherRejectReduceEventExchange(
    common::cmd::OrderCommand *cmd, common::MatcherTradeEvent *ev,
    const common::CoreSymbolSpecification *spec, bool takerSell,
    common::UserProfile *taker) {
  if (takerSell) {
    taker->accounts[spec->baseCurrency] +=
        utils::CoreArithmeticUtils::CalculateAmountAsk(ev->size, spec);
  } else {
    if (cmd->command == common::cmd::OrderCommandType::PLACE_ORDER &&
        cmd->orderType == common::OrderType::FOK_BUDGET) {
      taker->accounts[spec->quoteCurrency] +=
          utils::CoreArithmeticUtils::CalculateAmountBidTakerFeeForBudget(
              ev->size, ev->price, spec);
    } else {
      taker->accounts[spec->quoteCurrency] +=
          utils::CoreArithmeticUtils::CalculateAmountBidTakerFee(
              ev->size, ev->bidderHoldPrice, spec);
    }
  }
}

void RiskEngine::HandleMatcherEventsExchangeSell(
    common::MatcherTradeEvent *ev, const common::CoreSymbolSpecification *spec,
    common::UserProfile *taker) {
  // Simplified implementation - process trade events for sell orders
  // TODO: Implement full logic with maker handling
  while (ev != nullptr) {
    if (ev->eventType == common::MatcherEventType::TRADE) {
      // Update taker balance
      if (taker != nullptr) {
        int64_t quoteAmount = ev->size * ev->price * spec->quoteScaleK;
        taker->accounts[spec->quoteCurrency] += quoteAmount;
      }
    }
    ev = ev->nextEvent;
  }
}

void RiskEngine::HandleMatcherEventsExchangeBuy(
    common::MatcherTradeEvent *ev, const common::CoreSymbolSpecification *spec,
    common::UserProfile *taker, common::cmd::OrderCommand *cmd) {
  // Simplified implementation - process trade events for buy orders
  // TODO: Implement full logic with maker handling
  while (ev != nullptr) {
    if (ev->eventType == common::MatcherEventType::TRADE) {
      // Update taker balance
      if (taker != nullptr) {
        int64_t baseAmount = ev->size * spec->baseScaleK;
        taker->accounts[spec->baseCurrency] += baseAmount;
      }
    }
    ev = ev->nextEvent;
  }
}

void RiskEngine::HandleMatcherEventMargin(
    common::MatcherTradeEvent *ev, const common::CoreSymbolSpecification *spec,
    common::OrderAction takerAction, common::UserProfile *takerUp,
    common::SymbolPositionRecord *takerSpr) {
  if (takerUp != nullptr && takerSpr != nullptr) {
    if (ev->eventType == common::MatcherEventType::TRADE) {
      // update taker's position
      int64_t sizeOpen = takerSpr->UpdatePositionForMarginTrade(
          takerAction, ev->size, ev->price);
      int64_t fee = spec->takerFee * sizeOpen;
      takerUp->accounts[spec->quoteCurrency] -= fee;
      fees_[spec->quoteCurrency] += fee;
    } else if (ev->eventType == common::MatcherEventType::REJECT ||
               ev->eventType == common::MatcherEventType::REDUCE) {
      // for cancel/rejection only one party is involved
      takerSpr->PendingRelease(takerAction, ev->size);
    }

    if (takerSpr->IsEmpty()) {
      takerUp->positions.erase(spec->symbolId);
    }
  }

  if (ev->eventType == common::MatcherEventType::TRADE &&
      UidForThisHandler(ev->matchedOrderUid)) {
    // update maker's position
    auto *maker =
        userProfileService_->GetUserProfileOrAddSuspended(ev->matchedOrderUid);
    auto *makerSpr = maker->GetPositionRecordOrThrowEx(spec->symbolId);
    int64_t sizeOpen = makerSpr->UpdatePositionForMarginTrade(
        OppositeAction(takerAction), ev->size, ev->price);
    int64_t fee = spec->makerFee * sizeOpen;
    maker->accounts[spec->quoteCurrency] -= fee;
    fees_[spec->quoteCurrency] += fee;
    if (makerSpr->IsEmpty()) {
      maker->positions.erase(spec->symbolId);
    }
  }
}

common::OrderAction RiskEngine::OppositeAction(common::OrderAction action) {
  return (action == common::OrderAction::BID) ? common::OrderAction::ASK
                                              : common::OrderAction::BID;
}

// Explicit template instantiation
template std::optional<std::unique_ptr<common::api::reports::ReportResult>>
RiskEngine::HandleReportQuery<common::api::reports::ReportResult>(
    common::api::reports::ReportQuery<common::api::reports::ReportResult> *);

int32_t RiskEngine::GetStateHash() const {
  // Calculate state hash based on all stateful components
  // Note: Java version doesn't have explicit stateHash for RiskEngine,
  // but we implement it for consistency
  std::size_t hash = 0;
  if (symbolSpecificationProvider_ != nullptr) {
    hash ^=
        static_cast<std::size_t>(symbolSpecificationProvider_->GetStateHash())
        << 1;
  }
  if (userProfileService_ != nullptr) {
    hash ^= static_cast<std::size_t>(userProfileService_->GetStateHash()) << 2;
  }
  if (binaryCommandsProcessor_ != nullptr) {
    hash ^= static_cast<std::size_t>(binaryCommandsProcessor_->GetStateHash())
            << 3;
  }
  // Hash lastPriceCache, fees, adjustments, suspends
  for (const auto &pair : lastPriceCache_) {
    hash ^= (std::hash<int32_t>{}(pair.first) << 4) ^
            (std::hash<int64_t>{}(pair.second.askPrice) << 5) ^
            (std::hash<int64_t>{}(pair.second.bidPrice) << 6);
  }
  for (const auto &pair : fees_) {
    hash ^= (std::hash<int32_t>{}(pair.first) << 7) ^
            (std::hash<int64_t>{}(pair.second) << 8);
  }
  for (const auto &pair : adjustments_) {
    hash ^= (std::hash<int32_t>{}(pair.first) << 9) ^
            (std::hash<int64_t>{}(pair.second) << 10);
  }
  for (const auto &pair : suspends_) {
    hash ^= (std::hash<int32_t>{}(pair.first) << 11) ^
            (std::hash<int64_t>{}(pair.second) << 12);
  }
  return static_cast<int32_t>(hash);
}

void RiskEngine::WriteMarshallable(common::BytesOut &bytes) {
  // Write shardId and shardMask
  bytes.WriteInt(shardId_).WriteLong(shardMask_);

  // Write symbolSpecificationProvider
  if (symbolSpecificationProvider_ != nullptr) {
    symbolSpecificationProvider_->WriteMarshallable(bytes);
  }

  // Write userProfileService
  if (userProfileService_ != nullptr) {
    userProfileService_->WriteMarshallable(bytes);
  }

  // Write binaryCommandsProcessor
  if (binaryCommandsProcessor_ != nullptr) {
    binaryCommandsProcessor_->WriteMarshallable(bytes);
  }

  // Write lastPriceCache (int -> LastPriceCacheRecord)
  bytes.WriteInt(static_cast<int32_t>(lastPriceCache_.size()));
  for (const auto &pair : lastPriceCache_) {
    bytes.WriteInt(pair.first);
    pair.second.WriteMarshallable(bytes);
  }

  // Write fees (int -> long)
  utils::SerializationUtils::MarshallIntLongHashMap(fees_, bytes);

  // Write adjustments (int -> long)
  utils::SerializationUtils::MarshallIntLongHashMap(adjustments_, bytes);

  // Write suspends (int -> long)
  utils::SerializationUtils::MarshallIntLongHashMap(suspends_, bytes);
}

} // namespace processors
} // namespace core
} // namespace exchange
