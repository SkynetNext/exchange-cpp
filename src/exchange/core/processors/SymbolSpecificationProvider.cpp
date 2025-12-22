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

#include <exchange/core/processors/SymbolSpecificationProvider.h>
#include <functional>

namespace exchange {
namespace core {
namespace processors {

SymbolSpecificationProvider::SymbolSpecificationProvider() {}

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
  // Hash all symbol specifications
  std::size_t hash = 0;
  for (const auto &pair : symbolSpecs_) {
    std::size_t h1 = std::hash<int32_t>{}(pair.first);
    std::size_t h2 =
        std::hash<const common::CoreSymbolSpecification *>{}(pair.second);
    if (pair.second != nullptr) {
      h2 ^= static_cast<std::size_t>(pair.second->GetStateHash());
    }
    hash ^= (h1 << 1) ^ (h2 << 2);
  }
  return static_cast<int32_t>(hash);
}

} // namespace processors
} // namespace core
} // namespace exchange
