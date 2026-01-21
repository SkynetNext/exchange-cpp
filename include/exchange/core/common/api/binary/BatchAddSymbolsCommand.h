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
#include <vector>
#include "../../BytesIn.h"
#include "../../CoreSymbolSpecification.h"
#include "BinaryCommandType.h"
#include "BinaryDataCommand.h"

namespace exchange::core::common::api::binary {

/**
 * BatchAddSymbolsCommand - batch add symbols command
 */
class BatchAddSymbolsCommand : public BinaryDataCommand {
public:
  // symbol ID -> CoreSymbolSpecification
  ankerl::unordered_dense::map<int32_t, const CoreSymbolSpecification*> symbols;

  explicit BatchAddSymbolsCommand(
    const ankerl::unordered_dense::map<int32_t, const CoreSymbolSpecification*>& symbols)
    : symbols(symbols) {}

  explicit BatchAddSymbolsCommand(const CoreSymbolSpecification* symbol) {
    if (symbol != nullptr) {
      symbols[symbol->symbolId] = symbol;
    }
  }

  explicit BatchAddSymbolsCommand(const std::vector<const CoreSymbolSpecification*>& collection) {
    for (const auto* spec : collection) {
      if (spec != nullptr) {
        symbols[spec->symbolId] = spec;
      }
    }
  }

  explicit BatchAddSymbolsCommand(BytesIn& bytes);

  int32_t GetBinaryCommandTypeCode() const override {
    return static_cast<int32_t>(BinaryCommandType::ADD_SYMBOLS);
  }

  void WriteMarshallable(BytesOut& bytes) const override;
};

}  // namespace exchange::core::common::api::binary
