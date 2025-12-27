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

#include "OrderBookBaseTest.h"

namespace exchange {
namespace core {
namespace tests {
namespace orderbook {

/**
 * OrderBookDirectImplTest - base class for Direct implementation tests
 * Adds additional tests specific to Direct implementation
 */
class OrderBookDirectImplTest : public OrderBookBaseTest {
protected:
  // Additional test methods for Direct implementation
  void TestSequentialAsks();
  void TestSequentialBids();
  void TestMultipleCommandsCompare();
};

} // namespace orderbook
} // namespace tests
} // namespace core
} // namespace exchange
