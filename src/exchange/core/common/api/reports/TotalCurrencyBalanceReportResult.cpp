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

#include <exchange/core/common/BytesIn.h>
#include <exchange/core/common/BytesOut.h>
#include <exchange/core/common/api/reports/TotalCurrencyBalanceReportResult.h>
#include <exchange/core/utils/SerializationUtils.h>

namespace exchange {
namespace core {
namespace common {
namespace api {
namespace reports {

TotalCurrencyBalanceReportResult *
TotalCurrencyBalanceReportResult::CreateEmpty() {
  // Match Java: createEmpty()
  return new TotalCurrencyBalanceReportResult(
      nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr);
}

TotalCurrencyBalanceReportResult *
TotalCurrencyBalanceReportResult::OfOrderBalances(
    const ankerl::unordered_dense::map<int32_t, int64_t> &currencyBalance) {
  // Match Java: ofOrderBalances()
  auto *ordersBalances =
      new ankerl::unordered_dense::map<int32_t, int64_t>(currencyBalance);
  return new TotalCurrencyBalanceReportResult(
      nullptr, nullptr, nullptr, nullptr, ordersBalances, nullptr, nullptr);
}

TotalCurrencyBalanceReportResult::TotalCurrencyBalanceReportResult(
    BytesIn &bytes) {
  // Match Java: TotalCurrencyBalanceReportResult(final BytesIn bytesIn)
  accountBalances =
      bytes.ReadBoolean()
          ? new ankerl::unordered_dense::map<int32_t, int64_t>(
                utils::SerializationUtils::ReadIntLongHashMap(bytes))
          : nullptr;
  fees = bytes.ReadBoolean()
             ? new ankerl::unordered_dense::map<int32_t, int64_t>(
                   utils::SerializationUtils::ReadIntLongHashMap(bytes))
             : nullptr;
  adjustments = bytes.ReadBoolean()
                    ? new ankerl::unordered_dense::map<int32_t, int64_t>(
                          utils::SerializationUtils::ReadIntLongHashMap(bytes))
                    : nullptr;
  suspends = bytes.ReadBoolean()
                 ? new ankerl::unordered_dense::map<int32_t, int64_t>(
                       utils::SerializationUtils::ReadIntLongHashMap(bytes))
                 : nullptr;
  ordersBalances =
      bytes.ReadBoolean()
          ? new ankerl::unordered_dense::map<int32_t, int64_t>(
                utils::SerializationUtils::ReadIntLongHashMap(bytes))
          : nullptr;
  openInterestLong =
      bytes.ReadBoolean()
          ? new ankerl::unordered_dense::map<int32_t, int64_t>(
                utils::SerializationUtils::ReadIntLongHashMap(bytes))
          : nullptr;
  openInterestShort =
      bytes.ReadBoolean()
          ? new ankerl::unordered_dense::map<int32_t, int64_t>(
                utils::SerializationUtils::ReadIntLongHashMap(bytes))
          : nullptr;
}

void TotalCurrencyBalanceReportResult::WriteMarshallable(
    BytesOut &bytes) const {
  // Match Java: writeMarshallable()
  // SerializationUtils.marshallNullable(accountBalances, bytes,
  // SerializationUtils::marshallIntLongHashMap)
  using MapType = ankerl::unordered_dense::map<int32_t, int64_t>;

  bytes.WriteBoolean(accountBalances != nullptr);
  if (accountBalances != nullptr) {
    utils::SerializationUtils::MarshallIntLongHashMap(*accountBalances, bytes);
  }

  bytes.WriteBoolean(fees != nullptr);
  if (fees != nullptr) {
    utils::SerializationUtils::MarshallIntLongHashMap(*fees, bytes);
  }

  bytes.WriteBoolean(adjustments != nullptr);
  if (adjustments != nullptr) {
    utils::SerializationUtils::MarshallIntLongHashMap(*adjustments, bytes);
  }

  bytes.WriteBoolean(suspends != nullptr);
  if (suspends != nullptr) {
    utils::SerializationUtils::MarshallIntLongHashMap(*suspends, bytes);
  }

  bytes.WriteBoolean(ordersBalances != nullptr);
  if (ordersBalances != nullptr) {
    utils::SerializationUtils::MarshallIntLongHashMap(*ordersBalances, bytes);
  }

  bytes.WriteBoolean(openInterestLong != nullptr);
  if (openInterestLong != nullptr) {
    utils::SerializationUtils::MarshallIntLongHashMap(*openInterestLong, bytes);
  }

  bytes.WriteBoolean(openInterestShort != nullptr);
  if (openInterestShort != nullptr) {
    utils::SerializationUtils::MarshallIntLongHashMap(*openInterestShort,
                                                      bytes);
  }
}

std::unique_ptr<TotalCurrencyBalanceReportResult>
TotalCurrencyBalanceReportResult::Merge(const std::vector<BytesIn *> &pieces) {
  // Match Java: merge(final Stream<BytesIn> pieces)
  if (pieces.empty()) {
    return std::unique_ptr<TotalCurrencyBalanceReportResult>(CreateEmpty());
  }

  // Start with first piece
  auto result = std::make_unique<TotalCurrencyBalanceReportResult>(*pieces[0]);

  // Merge remaining pieces
  for (size_t i = 1; i < pieces.size(); i++) {
    auto next = std::make_unique<TotalCurrencyBalanceReportResult>(*pieces[i]);

    // Merge all maps using MergeSum
    result->accountBalances =
        new ankerl::unordered_dense::map<int32_t, int64_t>(
            utils::SerializationUtils::MergeSum(result->accountBalances,
                                                next->accountBalances));
    result->fees = new ankerl::unordered_dense::map<int32_t, int64_t>(
        utils::SerializationUtils::MergeSum(result->fees, next->fees));
    result->adjustments = new ankerl::unordered_dense::map<int32_t, int64_t>(
        utils::SerializationUtils::MergeSum(result->adjustments,
                                            next->adjustments));
    result->suspends = new ankerl::unordered_dense::map<int32_t, int64_t>(
        utils::SerializationUtils::MergeSum(result->suspends, next->suspends));
    result->ordersBalances = new ankerl::unordered_dense::map<int32_t, int64_t>(
        utils::SerializationUtils::MergeSum(result->ordersBalances,
                                            next->ordersBalances));
    result->openInterestLong =
        new ankerl::unordered_dense::map<int32_t, int64_t>(
            utils::SerializationUtils::MergeSum(result->openInterestLong,
                                                next->openInterestLong));
    result->openInterestShort =
        new ankerl::unordered_dense::map<int32_t, int64_t>(
            utils::SerializationUtils::MergeSum(result->openInterestShort,
                                                next->openInterestShort));
  }

  return result;
}

} // namespace reports
} // namespace api
} // namespace common
} // namespace core
} // namespace exchange
