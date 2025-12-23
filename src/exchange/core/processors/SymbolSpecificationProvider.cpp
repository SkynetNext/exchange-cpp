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
#include <exchange/core/processors/SymbolSpecificationProvider.h>
#include <exchange/core/utils/HashingUtils.h>
#include <exchange/core/utils/SerializationUtils.h>

namespace exchange {
namespace core {
namespace processors {

SymbolSpecificationProvider::SymbolSpecificationProvider() {}

SymbolSpecificationProvider::SymbolSpecificationProvider(
    common::BytesIn *bytes) {
  if (bytes == nullptr) {
    throw std::invalid_argument("BytesIn cannot be nullptr");
  }
  // Read symbolSpecs (int -> CoreSymbolSpecification*)
  int length = bytes->ReadInt();
  for (int i = 0; i < length; i++) {
    int32_t symbolId = bytes->ReadInt();
    common::CoreSymbolSpecification *spec =
        new common::CoreSymbolSpecification(*bytes);
    symbolSpecs_[symbolId] = spec;
  }
}

bool SymbolSpecificationProvider::AddSymbol(
    const common::CoreSymbolSpecification *symbolSpec) {
  if (symbolSpec == nullptr) {
    return false;
  }

  int32_t symbolId = symbolSpec->symbolId;
  if (symbolSpecs_.find(symbolId) != symbolSpecs_.end()) {
    return false; // Symbol already exists
  }

  symbolSpecs_[symbolId] = symbolSpec;
  return true;
}

const common::CoreSymbolSpecification *
SymbolSpecificationProvider::GetSymbolSpecification(int32_t symbol) const {
  auto it = symbolSpecs_.find(symbol);
  return (it != symbolSpecs_.end()) ? it->second : nullptr;
}

void SymbolSpecificationProvider::RegisterSymbol(
    int32_t symbol, const common::CoreSymbolSpecification *spec) {
  symbolSpecs_[symbol] = spec;
}

void SymbolSpecificationProvider::Reset() { symbolSpecs_.clear(); }

int32_t SymbolSpecificationProvider::GetStateHash() const {
  // Use HashingUtils::StateHash to match Java implementation
  // Convert const map to non-const for HashingUtils (safe cast)
  ankerl::unordered_dense::map<int32_t, common::CoreSymbolSpecification *>
      nonConstMap;
  for (const auto &pair : symbolSpecs_) {
    nonConstMap[pair.first] =
        const_cast<common::CoreSymbolSpecification *>(pair.second);
  }
  return utils::HashingUtils::StateHash(nonConstMap);
}

void SymbolSpecificationProvider::WriteMarshallable(common::BytesOut &bytes) {
  // Write symbolSpecs (int -> CoreSymbolSpecification*)
  // Convert const map to non-const for serialization (safe cast)
  ankerl::unordered_dense::map<int32_t, common::CoreSymbolSpecification *>
      nonConstMap;
  for (const auto &pair : symbolSpecs_) {
    nonConstMap[pair.first] =
        const_cast<common::CoreSymbolSpecification *>(pair.second);
  }
  utils::SerializationUtils::MarshallIntHashMap(nonConstMap, bytes);
}

} // namespace processors
} // namespace core
} // namespace exchange
