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

#include <exchange/core/common/BytesIn.h>
#include <exchange/core/common/BytesOut.h>
#include <exchange/core/common/api/binary/BatchAddAccountsCommand.h>
#include <exchange/core/common/api/binary/BinaryDataCommandFactory.h>
#include <exchange/core/utils/SerializationUtils.h>

namespace exchange {
namespace core {
namespace common {
namespace api {
namespace binary {

using namespace utils;

REGISTER_BINARY_COMMAND_TYPE(BatchAddAccountsCommand,
                             BinaryCommandType::ADD_ACCOUNTS);

BatchAddAccountsCommand::BatchAddAccountsCommand(BytesIn &bytes) {
  // Read LongObjectHashMap<IntLongHashMap>
  // Java: users = SerializationUtils.readLongHashMap(bytes, c ->
  // SerializationUtils.readIntLongHashMap(bytes));
  auto tempMap = SerializationUtils::ReadLongHashMap<
      ankerl::unordered_dense::map<int32_t, int64_t>>(
      bytes,
      [](BytesIn &b) -> ankerl::unordered_dense::map<int32_t, int64_t> * {
        auto *map = new ankerl::unordered_dense::map<int32_t, int64_t>(
            SerializationUtils::ReadIntLongHashMap(b));
        return map;
      });

  // Convert from map<int64_t, map<int32_t, int64_t>*> to map<int64_t,
  // map<int32_t, int64_t>>
  for (const auto &pair : tempMap) {
    if (pair.second != nullptr) {
      users[pair.first] = *pair.second;
      delete pair.second;
    }
  }
}

void BatchAddAccountsCommand::WriteMarshallable(BytesOut &bytes) const {
  // Match Java: SerializationUtils.marshallLongHashMap(users,
  // SerializationUtils::marshallIntLongHashMap, bytes);
  bytes.WriteInt(static_cast<int32_t>(users.size()));
  for (const auto &pair : users) {
    bytes.WriteLong(pair.first);
    SerializationUtils::MarshallIntLongHashMap(pair.second, bytes);
  }
}

} // namespace binary
} // namespace api
} // namespace common
} // namespace core
} // namespace exchange
