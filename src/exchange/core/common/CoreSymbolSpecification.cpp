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
#include <exchange/core/common/SymbolType.h>
#include <functional>
#include <sstream>

namespace exchange::core::common {

CoreSymbolSpecification::CoreSymbolSpecification(BytesIn& bytes)
    : symbolId(bytes.ReadInt())
    , type(SymbolTypeFromCode(bytes.ReadByte()))
    , baseCurrency(bytes.ReadInt())
    , quoteCurrency(bytes.ReadInt())
    , baseScaleK(bytes.ReadLong())
    , quoteScaleK(bytes.ReadLong())
    , takerFee(bytes.ReadLong())
    , makerFee(bytes.ReadLong())
    , marginBuy(bytes.ReadLong())
    , marginSell(bytes.ReadLong()) {}

CoreSymbolSpecification::CoreSymbolSpecification(int32_t symbolId,
                                                 SymbolType type,
                                                 int32_t baseCurrency,
                                                 int32_t quoteCurrency,
                                                 int64_t baseScaleK,
                                                 int64_t quoteScaleK,
                                                 int64_t takerFee,
                                                 int64_t makerFee,
                                                 int64_t marginBuy,
                                                 int64_t marginSell)
    : symbolId(symbolId)
    , type(type)
    , baseCurrency(baseCurrency)
    , quoteCurrency(quoteCurrency)
    , baseScaleK(baseScaleK)
    , quoteScaleK(quoteScaleK)
    , takerFee(takerFee)
    , makerFee(makerFee)
    , marginBuy(marginBuy)
    , marginSell(marginSell) {}

int32_t CoreSymbolSpecification::GetStateHash() const {
    std::size_t h1 = std::hash<int32_t>{}(symbolId);
    std::size_t h2 = std::hash<uint8_t>{}(SymbolTypeToCode(type));
    std::size_t h3 = std::hash<int32_t>{}(baseCurrency);
    std::size_t h4 = std::hash<int32_t>{}(quoteCurrency);
    std::size_t h5 = std::hash<int64_t>{}(baseScaleK);
    std::size_t h6 = std::hash<int64_t>{}(quoteScaleK);
    std::size_t h7 = std::hash<int64_t>{}(takerFee);
    std::size_t h8 = std::hash<int64_t>{}(makerFee);
    std::size_t h9 = std::hash<int64_t>{}(marginBuy);
    std::size_t h10 = std::hash<int64_t>{}(marginSell);

    return static_cast<int32_t>(h1 ^ (h2 << 1) ^ (h3 << 2) ^ (h4 << 3) ^ (h5 << 4) ^ (h6 << 5)
                                ^ (h7 << 6) ^ (h8 << 7) ^ (h9 << 8) ^ (h10 << 9));
}

std::string CoreSymbolSpecification::ToString() const {
    std::ostringstream oss;
    oss << "CoreSymbolSpecification{symbolId=" << symbolId
        << ", type=" << static_cast<int>(SymbolTypeToCode(type))
        << ", baseCurrency=" << baseCurrency << ", quoteCurrency=" << quoteCurrency
        << ", baseScaleK=" << baseScaleK << ", quoteScaleK=" << quoteScaleK
        << ", takerFee=" << takerFee << ", makerFee=" << makerFee << ", marginBuy=" << marginBuy
        << ", marginSell=" << marginSell << "}";
    return oss.str();
}

void CoreSymbolSpecification::WriteMarshallable(BytesOut& bytes) const {
    bytes.WriteInt(symbolId);
    bytes.WriteByte(static_cast<int8_t>(type));
    bytes.WriteInt(baseCurrency);
    bytes.WriteInt(quoteCurrency);
    bytes.WriteLong(baseScaleK);
    bytes.WriteLong(quoteScaleK);
    bytes.WriteLong(takerFee);
    bytes.WriteLong(makerFee);
    bytes.WriteLong(marginBuy);
    bytes.WriteLong(marginSell);
}

}  // namespace exchange::core::common
