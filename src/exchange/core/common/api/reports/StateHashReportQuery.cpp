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

#include <exchange/core/common/BytesOut.h>
#include <exchange/core/common/api/reports/ReportQueryFactory.h>
#include <exchange/core/common/api/reports/StateHashReportQuery.h>
#include <exchange/core/common/api/reports/StateHashReportResult.h>
#include <exchange/core/processors/MatchingEngineRouter.h>
#include <exchange/core/processors/RiskEngine.h>
#include <exchange/core/utils/HashingUtils.h>
#include <functional>

namespace exchange::core::common::api::reports {

// Use fully qualified names to avoid namespace ambiguity
using MatchingEngineRouterType = ::exchange::core::processors::MatchingEngineRouter;
using RiskEngineType = ::exchange::core::processors::RiskEngine;

REGISTER_REPORT_QUERY_TYPE(StateHashReportQuery, ReportType::STATE_HASH);

void StateHashReportQuery::WriteMarshallable(BytesOut& bytes) const {
    // Match Java: do nothing (empty implementation)
    // StateHashReportQuery has no fields to serialize
}

std::unique_ptr<StateHashReportResult> StateHashReportQuery::CreateResult(
    const std::vector<BytesIn*>& sections) {
    // Match Java: StateHashReportResult.merge(sections);
    return StateHashReportResult::Merge(sections);
}

std::optional<std::unique_ptr<StateHashReportResult>> StateHashReportQuery::Process(
    MatchingEngineRouterType* matchingEngine) {
    // Match Java: process(MatchingEngineRouter matchingEngine)
    std::map<StateHashReportResult::SubmoduleKey, int32_t> hashCodes;

    const int32_t moduleId = matchingEngine->GetShardId();

    hashCodes[StateHashReportResult::CreateKey(
        moduleId, StateHashReportResult::SubmoduleType::MATCHING_BINARY_CMD_PROCESSOR)] =
        matchingEngine->GetBinaryCommandsProcessor()->GetStateHash();

    // For OrderBooks - use StateHashStream (vector of pointers)
    const auto& orderBooks = matchingEngine->GetOrderBooks();
    hashCodes[StateHashReportResult::CreateKey(
        moduleId, StateHashReportResult::SubmoduleType::MATCHING_ORDER_BOOKS)] =
        utils::HashingUtils::StateHashStream(orderBooks);

    hashCodes[StateHashReportResult::CreateKey(
        moduleId, StateHashReportResult::SubmoduleType::MATCHING_SHARD_MASK)] =
        static_cast<int32_t>(std::hash<int64_t>{}(matchingEngine->GetShardMask()));

    return std::make_optional(std::make_unique<StateHashReportResult>(hashCodes));
}

std::optional<std::unique_ptr<StateHashReportResult>> StateHashReportQuery::Process(
    RiskEngineType* riskEngine) {
    // Match Java: process(RiskEngine riskEngine)
    std::map<StateHashReportResult::SubmoduleKey, int32_t> hashCodes;

    const int32_t moduleId = riskEngine->GetShardId();

    hashCodes[StateHashReportResult::CreateKey(
        moduleId, StateHashReportResult::SubmoduleType::RISK_SYMBOL_SPEC_PROVIDER)] =
        riskEngine->GetSymbolSpecificationProvider()->GetStateHash();

    hashCodes[StateHashReportResult::CreateKey(
        moduleId, StateHashReportResult::SubmoduleType::RISK_USER_PROFILE_SERVICE)] =
        riskEngine->GetUserProfileService()->GetStateHash();

    hashCodes[StateHashReportResult::CreateKey(
        moduleId, StateHashReportResult::SubmoduleType::RISK_BINARY_CMD_PROCESSOR)] =
        riskEngine->GetBinaryCommandsProcessor()->GetStateHash();

    // For LastPriceCache, Fees, Adjustments, Suspends - calculate hash manually
    // Match Java: HashingUtils.stateHash() or .hashCode()
    const auto& lastPriceCache = riskEngine->GetLastPriceCache();
    std::size_t lastPriceCacheHash = 0;
    for (const auto& pair : lastPriceCache) {
        lastPriceCacheHash ^= (std::hash<int32_t>{}(pair.first) << 1)
                              ^ (std::hash<int64_t>{}(pair.second.askPrice) << 2)
                              ^ (std::hash<int64_t>{}(pair.second.bidPrice) << 3);
    }
    hashCodes[StateHashReportResult::CreateKey(
        moduleId, StateHashReportResult::SubmoduleType::RISK_LAST_PRICE_CACHE)] =
        static_cast<int32_t>(lastPriceCacheHash);

    const auto& fees = riskEngine->GetFees();
    std::size_t feesHash = 0;
    for (const auto& pair : fees) {
        feesHash ^=
            (std::hash<int32_t>{}(pair.first) << 4) ^ (std::hash<int64_t>{}(pair.second) << 5);
    }
    hashCodes[StateHashReportResult::CreateKey(moduleId,
                                               StateHashReportResult::SubmoduleType::RISK_FEES)] =
        static_cast<int32_t>(feesHash);

    const auto& adjustments = riskEngine->GetAdjustments();
    std::size_t adjustmentsHash = 0;
    for (const auto& pair : adjustments) {
        adjustmentsHash ^=
            (std::hash<int32_t>{}(pair.first) << 6) ^ (std::hash<int64_t>{}(pair.second) << 7);
    }
    hashCodes[StateHashReportResult::CreateKey(
        moduleId, StateHashReportResult::SubmoduleType::RISK_ADJUSTMENTS)] =
        static_cast<int32_t>(adjustmentsHash);

    const auto& suspends = riskEngine->GetSuspends();
    std::size_t suspendsHash = 0;
    for (const auto& pair : suspends) {
        suspendsHash ^=
            (std::hash<int32_t>{}(pair.first) << 8) ^ (std::hash<int64_t>{}(pair.second) << 9);
    }
    hashCodes[StateHashReportResult::CreateKey(
        moduleId, StateHashReportResult::SubmoduleType::RISK_SUSPENDS)] =
        static_cast<int32_t>(suspendsHash);

    hashCodes[StateHashReportResult::CreateKey(
        moduleId, StateHashReportResult::SubmoduleType::RISK_SHARD_MASK)] =
        static_cast<int32_t>(std::hash<int64_t>{}(riskEngine->GetShardMask()));

    return std::make_optional(std::make_unique<StateHashReportResult>(hashCodes));
}

}  // namespace exchange::core::common::api::reports
