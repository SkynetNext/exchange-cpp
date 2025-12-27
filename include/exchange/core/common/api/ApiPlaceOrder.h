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

#include "../OrderAction.h"
#include "../OrderType.h"
#include "ApiCommand.h"
#include <cstdint>

namespace exchange {
namespace core {
namespace common {
namespace api {

/**
 * ApiPlaceOrder - place a new order command
 */
class ApiPlaceOrder : public ApiCommand {
public:
  int64_t price;
  int64_t size;
  int64_t orderId;
  OrderAction action;
  OrderType orderType;
  int64_t uid;
  int32_t symbol;
  int32_t userCookie;
  int64_t reservePrice;

  ApiPlaceOrder(int64_t price, int64_t size, int64_t orderId,
                OrderAction action, OrderType orderType, int64_t uid,
                int32_t symbol, int32_t userCookie, int64_t reservePrice)
      : price(price), size(size), orderId(orderId), action(action),
        orderType(orderType), uid(uid), symbol(symbol), userCookie(userCookie),
        reservePrice(reservePrice) {}
};

} // namespace api
} // namespace common
} // namespace core
} // namespace exchange
