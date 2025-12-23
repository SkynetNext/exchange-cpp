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

#include "../common/CoreSymbolSpecification.h"
#include "../common/StateHash.h"
#include "../common/WriteBytesMarshallable.h"
#include <ankerl/unordered_dense.h>
#include <cstdint>

namespace exchange {
namespace core {
namespace processors {

/**
 * SymbolSpecificationProvider - manages symbol specifications
 * Maps symbol ID to CoreSymbolSpecification
 */
class SymbolSpecificationProvider : public common::StateHash,
                                    public common::WriteBytesMarshallable {
public:
  SymbolSpecificationProvider();

  /**
   * Constructor from BytesIn (deserialization)
   */
  SymbolSpecificationProvider(common::BytesIn *bytes);

  /**
   * Add a new symbol specification
   * @return true if added, false if symbol already exists
   */
  bool AddSymbol(const common::CoreSymbolSpecification *symbolSpec);

  /**
   * Get symbol specification by symbol ID
   * @param symbol - symbol ID
   * @return symbol specification or nullptr if not found
   */
  const common::CoreSymbolSpecification *
  GetSymbolSpecification(int32_t symbol) const;

  /**
   * Register a new symbol specification (overwrites if exists)
   * @param symbol - symbol ID
   * @param spec - symbol specification
   */
  void RegisterSymbol(int32_t symbol,
                      const common::CoreSymbolSpecification *spec);

  /**
   * Reset state - clear all symbols
   */
  void Reset();

  // StateHash interface
  int32_t GetStateHash() const override;

  // WriteBytesMarshallable interface
  void WriteMarshallable(common::BytesOut &bytes) override;

private:
  // symbol ID -> CoreSymbolSpecification
  // Using ankerl::unordered_dense for better performance
  ankerl::unordered_dense::map<int32_t, const common::CoreSymbolSpecification *>
      symbolSpecs_;
};

} // namespace processors
} // namespace core
} // namespace exchange
