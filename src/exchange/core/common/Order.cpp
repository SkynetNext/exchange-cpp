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
#include <exchange/core/common/OrderAction.h>
#include <sstream>

namespace exchange {
namespace core {
namespace common {

Order::Order(BytesIn &bytes)
    : orderId(bytes.ReadLong()), price(bytes.ReadLong()),
      size(bytes.ReadLong()), filled(bytes.ReadLong()),
      reserveBidPrice(bytes.ReadLong()),
      action(OrderActionFromCode(bytes.ReadByte())), uid(bytes.ReadLong()),
      timestamp(bytes.ReadLong()) {}

int32_t Order::GetStateHash() const {
  // timestamp is not included in hashCode for repeatable results
  // Match Java Objects.hash(orderId, action, price, size, reserveBidPrice,
  // filled, uid) Objects.hash uses: result = 31 * result + (element == null ? 0
  // : element.hashCode())
  int32_t result = 1;
  result = 31 * result + static_cast<int32_t>(orderId ^ (orderId >> 32));
  result = 31 * result + static_cast<int32_t>(static_cast<int8_t>(action));
  result = 31 * result + static_cast<int32_t>(price ^ (price >> 32));
  result = 31 * result + static_cast<int32_t>(size ^ (size >> 32));
  result = 31 * result +
           static_cast<int32_t>(reserveBidPrice ^ (reserveBidPrice >> 32));
  result = 31 * result + static_cast<int32_t>(filled ^ (filled >> 32));
  result = 31 * result + static_cast<int32_t>(uid ^ (uid >> 32));
  return result;
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

void Order::WriteMarshallable(BytesOut &bytes) const {
  bytes.WriteLong(orderId);
  bytes.WriteLong(price);
  bytes.WriteLong(size);
  bytes.WriteLong(filled);
  bytes.WriteLong(reserveBidPrice);
  bytes.WriteByte(static_cast<int8_t>(action));
  bytes.WriteLong(uid);
  bytes.WriteLong(timestamp);
}

} // namespace common
} // namespace core
} // namespace exchange
