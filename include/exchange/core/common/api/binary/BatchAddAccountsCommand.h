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

#pragma once

#include "../../BytesIn.h"
#include "BinaryCommandType.h"
#include "BinaryDataCommand.h"
#include <ankerl/unordered_dense.h>
#include <cstdint>

namespace exchange {
namespace core {
namespace common {
namespace api {
namespace binary {

/**
 * BatchAddAccountsCommand - batch add accounts command
 * users: UID -> (currency -> balance)
 */
class BatchAddAccountsCommand : public BinaryDataCommand {
public:
  // UID -> (currency -> balance)
  ankerl::unordered_dense::map<int64_t,
                               ankerl::unordered_dense::map<int32_t, int64_t>>
      users;

  explicit BatchAddAccountsCommand(
      const ankerl::unordered_dense::map<
          int64_t, ankerl::unordered_dense::map<int32_t, int64_t>> &users)
      : users(users) {}

  explicit BatchAddAccountsCommand(BytesIn &bytes);

  int32_t GetBinaryCommandTypeCode() const override {
    return static_cast<int32_t>(BinaryCommandType::ADD_ACCOUNTS);
  }

  void WriteMarshallable(BytesOut &bytes) const override;
};

} // namespace binary
} // namespace api
} // namespace common
} // namespace core
} // namespace exchange
