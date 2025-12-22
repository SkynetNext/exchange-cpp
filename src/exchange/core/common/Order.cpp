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

#include <exchange/core/common/Order.h>
#include <functional>

namespace exchange {
namespace core {
namespace common {

int32_t Order::GetStateHash() const {
  // timestamp is not included in hashCode for repeatable results
  std::size_t h1 = std::hash<int64_t>{}(orderId);
  std::size_t h2 = std::hash<int64_t>{}(price);
  std::size_t h3 = std::hash<int64_t>{}(size);
  std::size_t h4 = std::hash<int64_t>{}(reserveBidPrice);
  std::size_t h5 = std::hash<int64_t>{}(filled);
  std::size_t h6 = std::hash<OrderAction>{}(action);
  std::size_t h7 = std::hash<int64_t>{}(uid);

  return static_cast<int32_t>(h1 ^ (h2 << 1) ^ (h3 << 2) ^ (h4 << 3) ^
                              (h5 << 4) ^ (h6 << 5) ^ (h7 << 6));
}

bool Order::operator==(const Order &other) const {
  if (this == &other) {
    return true;
  }

  // ignore timestamp
  return orderId == other.orderId && action == other.action &&
         price == other.price && size == other.size &&
         reserveBidPrice == other.reserveBidPrice && filled == other.filled &&
         uid == other.uid;
}

std::string Order::ToString() const {
  std::ostringstream oss;
  oss << "[" << orderId << " " << (action == OrderAction::ASK ? 'A' : 'B')
      << price << ":" << size << "F" << filled << " U" << uid << "]";
  return oss.str();
}

} // namespace common
} // namespace core
} // namespace exchange

