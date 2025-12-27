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
#include <exchange/core/common/CoreSymbolSpecification.h>
#include <exchange/core/common/api/binary/BatchAddSymbolsCommand.h>
#include <exchange/core/common/api/binary/BinaryDataCommandFactory.h>
#include <exchange/core/utils/SerializationUtils.h>

namespace exchange {
namespace core {
namespace common {
namespace api {
namespace binary {

using namespace utils;

REGISTER_BINARY_COMMAND_TYPE(BatchAddSymbolsCommand,
                             BinaryCommandType::ADD_SYMBOLS);

BatchAddSymbolsCommand::BatchAddSymbolsCommand(BytesIn &bytes) {
  // Read IntObjectHashMap<CoreSymbolSpecification>
  // Java: symbols = SerializationUtils.readIntHashMap(bytes,
  // CoreSymbolSpecification::new);
  auto tempMap = SerializationUtils::ReadIntHashMap<CoreSymbolSpecification>(
      bytes, [](BytesIn &b) -> CoreSymbolSpecification * {
        return new CoreSymbolSpecification(b);
      });

  // Convert from map<int32_t, CoreSymbolSpecification*> to map<int32_t, const
  // CoreSymbolSpecification*>
  for (const auto &pair : tempMap) {
    if (pair.second != nullptr) {
      symbols[pair.first] = pair.second;
    }
  }
}

void BatchAddSymbolsCommand::WriteMarshallable(BytesOut &bytes) const {
  // Match Java: SerializationUtils.marshallIntHashMap(symbols, bytes);
  // Convert from map<int32_t, const CoreSymbolSpecification*> to
  // map<int32_t, CoreSymbolSpecification*> for template
  ankerl::unordered_dense::map<int32_t, CoreSymbolSpecification *> tempMap;
  for (const auto &pair : symbols) {
    if (pair.second != nullptr) {
      tempMap[pair.first] = const_cast<CoreSymbolSpecification *>(pair.second);
    }
  }
  SerializationUtils::MarshallIntHashMap(tempMap, bytes);
}

} // namespace binary
} // namespace api
} // namespace common
} // namespace core
} // namespace exchange
