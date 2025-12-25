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
#include <exchange/core/common/Order.h>
#include <exchange/core/common/PositionDirection.h>
#include <exchange/core/common/UserStatus.h>
#include <exchange/core/common/api/reports/SingleUserReportResult.h>
#include <exchange/core/utils/SerializationUtils.h>

namespace exchange {
namespace core {
namespace common {
namespace api {
namespace reports {

SingleUserReportResult::Position::Position(BytesIn &bytes) {
  quoteCurrency = bytes.ReadInt();
  direction = PositionDirectionFromCode(bytes.ReadByte());
  openVolume = bytes.ReadLong();
  openPriceSum = bytes.ReadLong();
  profit = bytes.ReadLong();
  pendingSellSize = bytes.ReadLong();
  pendingBuySize = bytes.ReadLong();
}

void SingleUserReportResult::Position::WriteMarshallable(
    BytesOut &bytes) const {
  bytes.WriteInt(quoteCurrency);
  bytes.WriteByte(static_cast<int8_t>(GetMultiplier(direction)));
  bytes.WriteLong(openVolume);
  bytes.WriteLong(openPriceSum);
  bytes.WriteLong(profit);
  bytes.WriteLong(pendingSellSize);
  bytes.WriteLong(pendingBuySize);
}

void SingleUserReportResult::WriteMarshallable(BytesOut &bytes) const {
  // Match Java: writeMarshallable()
  bytes.WriteLong(uid);

  // userStatus
  bytes.WriteBoolean(userStatus != nullptr);
  if (userStatus != nullptr) {
    bytes.WriteByte(UserStatusToCode(*userStatus));
  }

  // accounts
  bytes.WriteBoolean(accounts != nullptr);
  if (accounts != nullptr) {
    utils::SerializationUtils::MarshallIntLongHashMap(*accounts, bytes);
  }

  // positions
  bytes.WriteBoolean(positions != nullptr);
  if (positions != nullptr) {
    bytes.WriteInt(static_cast<int32_t>(positions->size()));
    for (const auto &pair : *positions) {
      bytes.WriteInt(pair.first);
      pair.second.WriteMarshallable(bytes);
    }
  }

  // orders
  bytes.WriteBoolean(orders != nullptr);
  if (orders != nullptr) {
    bytes.WriteInt(static_cast<int32_t>(orders->size()));
    for (const auto &pair : *orders) {
      bytes.WriteInt(pair.first);
      bytes.WriteInt(static_cast<int32_t>(pair.second.size()));
      for (Order *order : pair.second) {
        if (order != nullptr) {
          order->WriteMarshallable(bytes);
        }
      }
    }
  }

  // queryExecutionStatus
  bytes.WriteInt(static_cast<int32_t>(queryExecutionStatus));
}

SingleUserReportResult::SingleUserReportResult(BytesIn &bytes) {
  // Match Java: private SingleUserReportResult(final BytesIn bytesIn)
  uid = bytes.ReadLong();
  if (bytes.ReadBoolean()) {
    UserStatus status = UserStatusFromCode(bytes.ReadByte());
    userStatus = new UserStatus(status);
  } else {
    userStatus = nullptr;
  }
  accounts =
      bytes.ReadBoolean()
          ? std::make_unique<ankerl::unordered_dense::map<int32_t, int64_t>>(
                utils::SerializationUtils::ReadIntLongHashMap(bytes))
          : nullptr;

  // positions
  if (bytes.ReadBoolean()) {
    positions =
        std::make_unique<ankerl::unordered_dense::map<int32_t, Position>>();
    int length = bytes.ReadInt();
    for (int i = 0; i < length; i++) {
      int32_t key = bytes.ReadInt();
      Position pos(bytes);
      positions->try_emplace(key, pos);
    }
  } else {
    positions = nullptr;
  }

  // orders
  if (bytes.ReadBoolean()) {
    orders = std::make_unique<
        ankerl::unordered_dense::map<int32_t, std::vector<Order *>>>();
    int length = bytes.ReadInt();
    for (int i = 0; i < length; i++) {
      int32_t key = bytes.ReadInt();
      int listLength = bytes.ReadInt();
      std::vector<Order *> orderList;
      orderList.reserve(listLength);
      for (int j = 0; j < listLength; j++) {
        orderList.push_back(new Order(bytes));
      }
      (*orders)[key] = orderList;
    }
  } else {
    orders = nullptr;
  }

  queryExecutionStatus = static_cast<QueryExecutionStatus>(bytes.ReadInt());
}

std::unique_ptr<SingleUserReportResult>
SingleUserReportResult::Merge(const std::vector<BytesIn *> &pieces) {
  // Match Java: merge(final Stream<BytesIn> pieces)
  // Java uses reduce with IDENTITY, so empty stream returns IDENTITY
  if (pieces.empty()) {
    // Return IDENTITY-like result (matches Java IDENTITY)
    return std::make_unique<SingleUserReportResult>();
  }

  // Start with first piece
  auto result = std::make_unique<SingleUserReportResult>(*pieces[0]);

  // Merge remaining pieces
  for (size_t i = 1; i < pieces.size(); i++) {
    auto next = std::make_unique<SingleUserReportResult>(*pieces[i]);

    // Merge logic (matches Java reduce)
    result->userStatus = utils::SerializationUtils::PreferNotNull(
        result->userStatus, next->userStatus);

    if (result->accounts == nullptr) {
      result->accounts = std::move(next->accounts);
    } else if (next->accounts != nullptr) {
      // Merge accounts (sum values)
      for (const auto &pair : *next->accounts) {
        (*result->accounts)[pair.first] += pair.second;
      }
    }

    if (result->positions == nullptr) {
      result->positions = std::move(next->positions);
    } else if (next->positions != nullptr) {
      // Merge positions (prefer non-null)
      for (const auto &pair : *next->positions) {
        if (result->positions->find(pair.first) == result->positions->end()) {
          (*result->positions)[pair.first] = pair.second;
        }
      }
    }

    if (result->orders == nullptr) {
      result->orders = std::move(next->orders);
    } else if (next->orders != nullptr) {
      // Merge orders (override)
      for (const auto &pair : *next->orders) {
        (*result->orders)[pair.first] = pair.second;
      }
    }

    // queryExecutionStatus: prefer non-OK
    if (result->queryExecutionStatus == QueryExecutionStatus::OK) {
      result->queryExecutionStatus = next->queryExecutionStatus;
    }
  }

  return result;
}

SingleUserReportResult *SingleUserReportResult::CreateFromMatchingEngine(
    int64_t uid,
    const ankerl::unordered_dense::map<int32_t, std::vector<Order *>> &orders) {
  // Match Java: createFromMatchingEngine(long uid,
  // IntObjectHashMap<List<Order>> orders) Returns: new
  // SingleUserReportResult(uid, null, null, null, orders,
  // QueryExecutionStatus.OK)
  auto *result = new SingleUserReportResult();
  result->uid = uid;
  result->userStatus = nullptr;
  result->accounts = nullptr;
  result->positions = nullptr;
  result->orders = std::make_unique<
      ankerl::unordered_dense::map<int32_t, std::vector<Order *>>>(orders);
  result->queryExecutionStatus = QueryExecutionStatus::OK;
  return result;
}

SingleUserReportResult *SingleUserReportResult::CreateFromRiskEngineFound(
    int64_t uid, UserStatus *userStatus,
    const ankerl::unordered_dense::map<int32_t, int64_t> &accounts,
    const ankerl::unordered_dense::map<int32_t, Position> &positions) {
  // Match Java: createFromRiskEngineFound(long uid, UserStatus userStatus,
  // IntLongHashMap accounts, IntObjectHashMap<Position> positions)
  // Returns: new SingleUserReportResult(uid, userStatus, accounts, positions,
  // null, QueryExecutionStatus.OK)
  auto *result = new SingleUserReportResult();
  result->uid = uid;
  result->userStatus = userStatus;
  result->accounts =
      std::make_unique<ankerl::unordered_dense::map<int32_t, int64_t>>(
          accounts);
  result->positions =
      std::make_unique<ankerl::unordered_dense::map<int32_t, Position>>(
          positions);
  result->orders = nullptr;
  result->queryExecutionStatus = QueryExecutionStatus::OK;
  return result;
}

std::unique_ptr<SingleUserReportResult>
SingleUserReportResult::CreateFromRiskEngineNotFound(int64_t uid) {
  // Match Java: createFromRiskEngineNotFound(long uid)
  // Returns: new SingleUserReportResult(uid, null, null, null, null,
  // QueryExecutionStatus.USER_NOT_FOUND)
  auto result = std::make_unique<SingleUserReportResult>();
  result->uid = uid;
  result->userStatus = nullptr;
  result->accounts = nullptr;
  result->positions = nullptr;
  result->orders = nullptr;
  result->queryExecutionStatus = QueryExecutionStatus::USER_NOT_FOUND;
  return result;
}

} // namespace reports
} // namespace api
} // namespace common
} // namespace core
} // namespace exchange
