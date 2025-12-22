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

#include "ApiCommand.h"
#include <cstdint>

namespace exchange {
namespace core {
namespace common {
namespace api {

/**
 * ApiReduceOrder - reduce size of an existing order
 */
class ApiReduceOrder : public ApiCommand {
public:
  int64_t orderId;
  int64_t uid;
  int32_t symbol;
  int64_t reduceSize;

  ApiReduceOrder(int64_t orderId, int64_t uid, int32_t symbol,
                 int64_t reduceSize)
      : orderId(orderId), uid(uid), symbol(symbol), reduceSize(reduceSize) {}
};

} // namespace api
} // namespace common
} // namespace core
} // namespace exchange
