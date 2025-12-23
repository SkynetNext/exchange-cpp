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
#include <exchange/core/common/SymbolType.h>
#include <exchange/core/common/api/binary/BatchAddAccountsCommand.h>
#include <exchange/core/common/api/binary/BatchAddSymbolsCommand.h>
#include <exchange/core/common/cmd/CommandResultCode.h>
#include <exchange/core/common/cmd/OrderCommand.h>
#include <exchange/core/common/cmd/OrderCommandType.h>
#include <exchange/core/orderbook/IOrderBook.h>
#include <exchange/core/orderbook/OrderBookEventsHelper.h>
#include <exchange/core/orderbook/OrderBookNaiveImpl.h>
#include <exchange/core/processors/BinaryCommandsProcessor.h>
#include <exchange/core/processors/MatchingEngineRouter.h>
#include <exchange/core/processors/SharedPool.h>
#include <exchange/core/processors/journaling/DiskSerializationProcessorConfiguration.h>
#include <exchange/core/utils/UnsafeUtils.h>
#include <stdexcept>

namespace exchange {
namespace core {
namespace processors {

MatchingEngineRouter::MatchingEngineRouter(
    int32_t shardId, int64_t numShards, OrderBookFactory orderBookFactory,
    SharedPool *sharedPool,
    const common::config::ExchangeConfiguration *exchangeCfg,
    journaling::ISerializationProcessor *serializationProcessor,
    SymbolSpecificationProvider *symbolSpecProvider)
    : shardId_(shardId), shardMask_(numShards - 1), exchangeId_(""),
      folder_(""), symbolSpecProvider_(symbolSpecProvider),
      orderBookFactory_(std::move(orderBookFactory)),
      serializationProcessor_(serializationProcessor),
      cfgMarginTradingEnabled_(false), cfgSendL2ForEveryCmd_(false),
      cfgL2RefreshDepth_(8), logDebug_(false) {

  if ((numShards & (numShards - 1)) != 0) {
    throw std::invalid_argument(
        "Invalid number of shards - must be power of 2");
  }

  // Create OrderBookEventsHelper with SharedPool
  if (sharedPool != nullptr) {
    eventsHelper_ = std::make_unique<orderbook::OrderBookEventsHelper>(
        [sharedPool]() { return sharedPool->GetChain(); });
  } else {
    eventsHelper_ = std::make_unique<orderbook::OrderBookEventsHelper>();
  }

  // Initialize object pools
  // Matches Java MatchingEngineRouter configuration (production pool)
  // TODO: Move to performance configuration
  objectsPool_ =
      std::unique_ptr<::exchange::core::collections::objpool::ObjectsPool>(
          ::exchange::core::collections::objpool::ObjectsPool::
              CreateProductionPool());

  // Read configuration from ExchangeConfiguration
  if (exchangeCfg != nullptr) {
    const auto &initStateCfg = exchangeCfg->initStateCfg;
    exchangeId_ = initStateCfg.exchangeId;
    folder_ = std::filesystem::path(
        journaling::DiskSerializationProcessorConfiguration::DEFAULT_FOLDER);

    const auto &ordersProcCfg = exchangeCfg->ordersProcessingCfg;
    cfgMarginTradingEnabled_ = (ordersProcCfg.marginTradingMode ==
                                common::config::OrdersProcessingConfiguration::
                                    MarginTradingMode::MARGIN_TRADING_ENABLED);

    const auto &perfCfg = exchangeCfg->performanceCfg;
    cfgSendL2ForEveryCmd_ = perfCfg.sendL2ForEveryCmd;
    cfgL2RefreshDepth_ = perfCfg.l2RefreshDepth;

    const auto &loggingCfg = exchangeCfg->loggingCfg;
    // Check if LOGGING_MATCHING_DEBUG is in logging levels
    logDebug_ = loggingCfg.Contains(common::config::LoggingConfiguration::
                                        LoggingLevel::LOGGING_MATCHING_DEBUG);

    // Try to load from snapshot
    if (journaling::ISerializationProcessor::CanLoadFromSnapshot(
            serializationProcessor_, &initStateCfg, shardId_,
            journaling::ISerializationProcessor::SerializedModuleType::
                MATCHING_ENGINE_ROUTER)) {
      // Load from snapshot
      auto deserialized = serializationProcessor_->LoadData<void *>(
          initStateCfg.snapshotId,
          journaling::ISerializationProcessor::SerializedModuleType::
              MATCHING_ENGINE_ROUTER,
          shardId_,
          [this, sharedPool, exchangeCfg](common::BytesIn *bytesIn) -> void * {
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

            // Deserialize BinaryCommandsProcessor
            binaryCommandsProcessor_ =
                std::make_unique<BinaryCommandsProcessor>(
                    [this](common::api::binary::BinaryDataCommand *msg) {
                      HandleBinaryMessage(msg);
                    },
                    nullptr, // ReportQueriesHandler - will be set up by
                             // BinaryCommandsProcessor
                    sharedPool, &exchangeCfg->reportsQueriesCfg, bytesIn,
                    shardId_ + 1024);

            // Deserialize orderBooks (int -> IOrderBook*)
            int orderBooksLength = bytesIn->ReadInt();
            for (int i = 0; i < orderBooksLength; i++) {
              int32_t symbolId = bytesIn->ReadInt();
              auto orderBook = orderbook::IOrderBook::Create(
                  bytesIn, objectsPool_.get(), eventsHelper_.get(),
                  &exchangeCfg->loggingCfg);
              orderBooks_[symbolId] = std::move(orderBook);
            }

            return nullptr; // Not used
          });
    } else {
      // Create BinaryCommandsProcessor normally
      binaryCommandsProcessor_ = std::make_unique<BinaryCommandsProcessor>(
          [this](common::api::binary::BinaryDataCommand *msg) {
            HandleBinaryMessage(msg);
          },
          nullptr, // ReportQueriesHandler - will be set up by
                   // BinaryCommandsProcessor
          sharedPool, &exchangeCfg->reportsQueriesCfg, shardId + 1024);
    }
  } else {
    // Create with minimal configuration
    binaryCommandsProcessor_ = std::make_unique<BinaryCommandsProcessor>(
        [this](common::api::binary::BinaryDataCommand *msg) {
          HandleBinaryMessage(msg);
        },
        nullptr, sharedPool, nullptr, shardId + 1024);
  }
}

void MatchingEngineRouter::ProcessOrder(int64_t seq,
                                        common::cmd::OrderCommand *cmd) {
  const auto command = cmd->command;

  // Handle matching commands (PLACE_ORDER, CANCEL_ORDER, etc.)
  if (command == common::cmd::OrderCommandType::MOVE_ORDER ||
      command == common::cmd::OrderCommandType::CANCEL_ORDER ||
      command == common::cmd::OrderCommandType::PLACE_ORDER ||
      command == common::cmd::OrderCommandType::REDUCE_ORDER ||
      command == common::cmd::OrderCommandType::ORDER_BOOK_REQUEST) {
    // Process specific symbol group only
    if (SymbolForThisHandler(cmd->symbol)) {
      ProcessMatchingCommand(cmd);
    }
    return;
  }

  // Handle binary data commands/queries
  if (command == common::cmd::OrderCommandType::BINARY_DATA_QUERY ||
      command == common::cmd::OrderCommandType::BINARY_DATA_COMMAND) {
    if (binaryCommandsProcessor_ != nullptr) {
      const auto resultCode = binaryCommandsProcessor_->AcceptBinaryFrame(cmd);
      if (shardId_ == 0) {
        cmd->resultCode = resultCode;
      }
    }
    return;
  }

  // Handle RESET command
  if (command == common::cmd::OrderCommandType::RESET) {
    // Process all symbol groups, only processor 0 writes result
    orderBooks_.clear();
    if (binaryCommandsProcessor_ != nullptr) {
      binaryCommandsProcessor_->Reset();
    }
    if (shardId_ == 0) {
      cmd->resultCode = common::cmd::CommandResultCode::SUCCESS;
    }
    return;
  }

  // Handle NOP command
  if (command == common::cmd::OrderCommandType::NOP) {
    if (shardId_ == 0) {
      cmd->resultCode = common::cmd::CommandResultCode::SUCCESS;
    }
    return;
  }

  // Handle PERSIST_STATE_MATCHING command
  if (command == common::cmd::OrderCommandType::PERSIST_STATE_MATCHING) {
    if (serializationProcessor_ != nullptr) {
      const bool isSuccess = serializationProcessor_->StoreData(
          cmd->orderId, seq, cmd->timestamp,
          journaling::ISerializationProcessor::SerializedModuleType::
              MATCHING_ENGINE_ROUTER,
          shardId_, this);
      // Send ACCEPTED because this is a first command in series.
      // Risk engine is second - so it will return SUCCESS
      utils::UnsafeUtils::SetResultVolatile(
          cmd, isSuccess, common::cmd::CommandResultCode::ACCEPTED,
          common::cmd::CommandResultCode::STATE_PERSIST_MATCHING_ENGINE_FAILED);
    } else {
      cmd->resultCode =
          common::cmd::CommandResultCode::STATE_PERSIST_MATCHING_ENGINE_FAILED;
    }
    return;
  }
}

orderbook::IOrderBook *MatchingEngineRouter::GetOrderBook(int32_t symbolId) {
  auto it = orderBooks_.find(symbolId);
  return (it != orderBooks_.end()) ? it->second.get() : nullptr;
}

void MatchingEngineRouter::AddSymbol(
    const common::CoreSymbolSpecification *spec) {
  if (spec == nullptr) {
    return;
  }

  // Check margin trading configuration
  if (spec->type != common::SymbolType::CURRENCY_EXCHANGE_PAIR &&
      !cfgMarginTradingEnabled_) {
    // Log warning: Margin symbols are not allowed
    // TODO: Add logging
    return;
  }

  if (orderBooks_.find(spec->symbolId) != orderBooks_.end()) {
    // OrderBook for symbol already exists
    // TODO: Log warning
    return;
  }

  // Create new order book using factory
  if (orderBookFactory_) {
    auto orderBook =
        orderBookFactory_(spec, objectsPool_.get(), eventsHelper_.get());
    orderBooks_[spec->symbolId] = std::move(orderBook);
  } else {
    // Fallback to naive implementation
    auto orderBook = std::make_unique<orderbook::OrderBookNaiveImpl>(
        spec, objectsPool_.get(), eventsHelper_.get());
    orderBooks_[spec->symbolId] = std::move(orderBook);
  }

  if (symbolSpecProvider_ != nullptr) {
    symbolSpecProvider_->AddSymbol(spec);
  }
}

void MatchingEngineRouter::HandleBinaryMessage(
    common::api::binary::BinaryDataCommand *message) {
  if (message == nullptr) {
    return;
  }

  // Handle BatchAddSymbolsCommand
  if (auto *batchAddSymbols =
          dynamic_cast<common::api::binary::BatchAddSymbolsCommand *>(
              message)) {
    for (const auto &pair : batchAddSymbols->symbols) {
      AddSymbol(pair.second);
    }
  }
  // Handle BatchAddAccountsCommand - do nothing (handled by RiskEngine)
  // else if (auto *batchAddAccounts = dynamic_cast<...>(message)) {
  //   // do nothing
  // }
}

bool MatchingEngineRouter::SymbolForThisHandler(int32_t symbolId) const {
  return (symbolId & shardMask_) == shardId_;
}

void MatchingEngineRouter::Reset() {
  orderBooks_.clear();
  if (binaryCommandsProcessor_ != nullptr) {
    binaryCommandsProcessor_->Reset();
  }
  if (symbolSpecProvider_ != nullptr) {
    symbolSpecProvider_->Reset();
  }
}

std::vector<orderbook::IOrderBook *>
MatchingEngineRouter::GetOrderBooks() const {
  std::vector<orderbook::IOrderBook *> result;
  result.reserve(orderBooks_.size());
  for (const auto &pair : orderBooks_) {
    result.push_back(pair.second.get());
  }
  return result;
}

void MatchingEngineRouter::ProcessMatchingCommand(
    common::cmd::OrderCommand *cmd) {
  orderbook::IOrderBook *orderBook = GetOrderBook(cmd->symbol);
  if (orderBook == nullptr) {
    cmd->resultCode =
        common::cmd::CommandResultCode::MATCHING_INVALID_ORDER_BOOK_ID;
    return;
  }

  // Process command using static helper
  orderbook::IOrderBook::ProcessCommand(orderBook, cmd);

  // Post market data if needed
  if ((cfgSendL2ForEveryCmd_ || (cmd->serviceFlags & 1) != 0) &&
      cmd->command != common::cmd::OrderCommandType::ORDER_BOOK_REQUEST &&
      cmd->resultCode == common::cmd::CommandResultCode::SUCCESS) {
    cmd->marketData = orderBook->GetL2MarketDataSnapshot(cfgL2RefreshDepth_);
  }
}

void MatchingEngineRouter::WriteMarshallable(common::BytesOut &bytes) {
  // Write shardId and shardMask
  bytes.WriteInt(shardId_).WriteLong(shardMask_);

  // Write binaryCommandsProcessor
  if (binaryCommandsProcessor_ != nullptr) {
    binaryCommandsProcessor_->WriteMarshallable(bytes);
  }

  // Write orderBooks
  // Convert unique_ptr map to raw pointer map for serialization
  bytes.WriteInt(static_cast<int32_t>(orderBooks_.size()));
  for (const auto &pair : orderBooks_) {
    bytes.WriteInt(pair.first);
    if (pair.second != nullptr) {
      pair.second->WriteMarshallable(bytes);
    }
  }
}

} // namespace processors
} // namespace core
} // namespace exchange
