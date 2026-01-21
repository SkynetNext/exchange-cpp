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

#include <ankerl/unordered_dense.h>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include "../collections/objpool/ObjectsPool.h"
#include "../common/BalanceAdjustmentType.h"
#include "../common/WriteBytesMarshallable.h"
#include "../common/api/binary/BinaryDataCommand.h"
#include "../common/api/reports/ReportQuery.h"
#include "../common/cmd/OrderCommand.h"
#include "BinaryCommandsProcessor.h"
#include "SharedPool.h"
#include "SymbolSpecificationProvider.h"
#include "UserProfileService.h"

namespace exchange::core {

namespace common::config {
class ExchangeConfiguration;
}

namespace processors {

// Forward declarations
namespace journaling {
class ISerializationProcessor;
}

/**
 * LastPriceCacheRecord - last price cache record
 */
struct LastPriceCacheRecord {
    int64_t askPrice = INT64_MAX;
    int64_t bidPrice = 0;

    LastPriceCacheRecord() = default;

    LastPriceCacheRecord(int64_t askPrice, int64_t bidPrice)
        : askPrice(askPrice), bidPrice(bidPrice) {}

    /**
     * Constructor from BytesIn (deserialization)
     */
    explicit LastPriceCacheRecord(common::BytesIn& bytes) {
        askPrice = bytes.ReadLong();
        bidPrice = bytes.ReadLong();
    }

    LastPriceCacheRecord AveragingRecord() const {
        LastPriceCacheRecord average;
        average.askPrice = (askPrice + bidPrice) >> 1;
        average.bidPrice = average.askPrice;
        return average;
    }

    static LastPriceCacheRecord Dummy() {
        return LastPriceCacheRecord(42, 42);
    }

    // Helper method for serialization
    void WriteMarshallable(common::BytesOut& bytes) const {
        bytes.WriteLong(askPrice);
        bytes.WriteLong(bidPrice);
    }
};

/**
 * RiskEngine - stateful risk engine
 * Handles risk management (R1 - Pre-hold, R2 - Release)
 */
class RiskEngine : public common::WriteBytesMarshallable, public common::StateHash {
 public:
    RiskEngine(int32_t shardId,
               int64_t numShards,
               journaling::ISerializationProcessor* serializationProcessor,
               SharedPool* sharedPool,
               const common::config::ExchangeConfiguration* exchangeConfiguration);

    /**
     * Pre-process command handler (R1 - Pre-hold)
     * @param seq - command sequence
     * @param cmd - command
     * @return true if caller should publish sequence even if batch was not
     * processed yet
     */
    bool PreProcessCommand(int64_t seq, common::cmd::OrderCommand* cmd);

    /**
     * Post-process command handler (R2 - Release)
     * Processes trade events from matching engine
     */
    void PostProcessCommand(int64_t seq, common::cmd::OrderCommand* cmd);

    /**
     * Check if UID belongs to this shard
     */
    bool UidForThisHandler(int64_t uid) const;

    /**
     * Get shard ID
     */
    int32_t GetShardId() const {
        return shardId_;
    }

    /**
     * Get shard mask
     */
    int64_t GetShardMask() const {
        return shardMask_;
    }

    /**
     * Get symbol specification provider
     */
    SymbolSpecificationProvider* GetSymbolSpecificationProvider() {
        return symbolSpecificationProvider_.get();
    }

    /**
     * Get user profile service
     */
    UserProfileService* GetUserProfileService() {
        return userProfileService_.get();
    }

    /**
     * Get binary commands processor
     */
    BinaryCommandsProcessor* GetBinaryCommandsProcessor() {
        return binaryCommandsProcessor_.get();
    }

    /**
     * Get last price cache
     */
    const ankerl::unordered_dense::map<int32_t, LastPriceCacheRecord>& GetLastPriceCache() const {
        return lastPriceCache_;
    }

    /**
     * Get fees
     */
    const ankerl::unordered_dense::map<int32_t, int64_t>& GetFees() const {
        return fees_;
    }

    /**
     * Get adjustments
     */
    const ankerl::unordered_dense::map<int32_t, int64_t>& GetAdjustments() const {
        return adjustments_;
    }

    /**
     * Get suspends
     */
    const ankerl::unordered_dense::map<int32_t, int64_t>& GetSuspends() const {
        return suspends_;
    }

    /**
     * Reset - clear all state
     */
    void Reset();

    // StateHash interface
    int32_t GetStateHash() const override;

    // WriteBytesMarshallable interface
    void WriteMarshallable(common::BytesOut& bytes) const override;

 private:
    int32_t shardId_;
    int64_t shardMask_;  // numShards - 1 (must be power of 2)

    std::string exchangeId_;  // TODO validate
    std::filesystem::path folder_;

    std::unique_ptr<SymbolSpecificationProvider> symbolSpecificationProvider_;
    std::unique_ptr<UserProfileService> userProfileService_;
    std::unique_ptr<BinaryCommandsProcessor> binaryCommandsProcessor_;
    std::unique_ptr<common::api::reports::ReportQueriesHandler> reportQueriesHandler_;

    journaling::ISerializationProcessor* serializationProcessor_;

    // Object pool for risk engine operations
    // Created in constructor with configuration matching Java version
    std::unique_ptr<::exchange::core::collections::objpool::ObjectsPool> objectsPool_;

    // symbol -> LastPriceCacheRecord
    ankerl::unordered_dense::map<int32_t, LastPriceCacheRecord> lastPriceCache_;

    // currency -> amount
    ankerl::unordered_dense::map<int32_t, int64_t> fees_;
    ankerl::unordered_dense::map<int32_t, int64_t> adjustments_;
    ankerl::unordered_dense::map<int32_t, int64_t> suspends_;

    bool cfgIgnoreRiskProcessing_;
    bool cfgMarginTradingEnabled_;
    bool logDebug_;

    /**
     * Place order risk check
     */
    common::cmd::CommandResultCode PlaceOrderRiskCheck(common::cmd::OrderCommand* cmd);

    /**
     * Place order (internal)
     */
    common::cmd::CommandResultCode PlaceOrder(common::cmd::OrderCommand* cmd,
                                              common::UserProfile* userProfile,
                                              const common::CoreSymbolSpecification* spec);

    /**
     * Place exchange order
     */
    common::cmd::CommandResultCode PlaceExchangeOrder(common::cmd::OrderCommand* cmd,
                                                      common::UserProfile* userProfile,
                                                      const common::CoreSymbolSpecification* spec);

    /**
     * Adjust balance
     */
    common::cmd::CommandResultCode AdjustBalance(
        int64_t uid,
        int32_t currency,
        int64_t amountDiff,
        int64_t fundingTransactionId,
        ::exchange::core::common::BalanceAdjustmentType adjustmentType);

    /**
     * Handle binary message
     */
    void HandleBinaryMessage(common::api::binary::BinaryDataCommand* message);

    /**
     * Handle report query
     */
    template <typename R>
    std::optional<std::unique_ptr<R>> HandleReportQuery(
        common::api::reports::ReportQuery<R>* reportQuery);

    // Make HandleReportQuery accessible to RiskEngineReportQueriesHandler
    friend class RiskEngineReportQueriesHandler;

    /**
     * Handle matcher reject/reduce event for exchange
     */
    void HandleMatcherRejectReduceEventExchange(common::cmd::OrderCommand* cmd,
                                                common::MatcherTradeEvent* ev,
                                                const common::CoreSymbolSpecification* spec,
                                                bool takerSell,
                                                common::UserProfile* taker);

    /**
     * Handle matcher events for exchange sell
     */
    void HandleMatcherEventsExchangeSell(common::MatcherTradeEvent* ev,
                                         const common::CoreSymbolSpecification* spec,
                                         common::UserProfile* taker);

    /**
     * Handle matcher events for exchange buy
     */
    void HandleMatcherEventsExchangeBuy(common::MatcherTradeEvent* ev,
                                        const common::CoreSymbolSpecification* spec,
                                        common::UserProfile* taker,
                                        common::cmd::OrderCommand* cmd);

    /**
     * Handle matcher event for margin trading
     */
    void HandleMatcherEventMargin(common::MatcherTradeEvent* ev,
                                  const common::CoreSymbolSpecification* spec,
                                  common::OrderAction takerAction,
                                  common::UserProfile* takerUp,
                                  common::SymbolPositionRecord* takerSpr);

    /**
     * Get opposite action
     */
    static common::OrderAction OppositeAction(common::OrderAction action);

    /**
     * Check if margin order can be placed
     */
    bool CanPlaceMarginOrder(common::cmd::OrderCommand* cmd,
                             common::UserProfile* userProfile,
                             const common::CoreSymbolSpecification* spec,
                             common::SymbolPositionRecord* position);

    /**
     * Remove position record
     */
    void RemovePositionRecord(common::SymbolPositionRecord* record,
                              common::UserProfile* userProfile);
};

}  // namespace processors
}  // namespace exchange::core
