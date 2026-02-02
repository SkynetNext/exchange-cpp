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

#include "../collections/objpool/ObjectPool.h"
#include "OrderBookDirectTypes.h"

namespace exchange::core::orderbook {

/**
 * ART pool profile: selects ArtPoolContext preset (matches old ObjectsPool configs).
 */
enum class ArtPoolProfile {
  /** Small capacity, fast startup (unit tests). */
  DefaultTest,
  /** Production: 32K/16K/8K/4K nodes (matches old CreateProductionPool). */
  Production,
  /** High load: 64K/32K/16K/8K nodes (matches old CreateHighLoadPool). */
  HighLoad,
  /** Large test: 262K/131K/65K/32K nodes (matches old CreateDefaultTestPool ART). */
  DefaultTestPool
};

/**
 * OrderBookPoolContext - Aggregates ObjectPools for OrderBookDirectImpl
 * Defined at orderbook level (not in OrderBookDirectImpl) to avoid
 * IOrderBook depending on concrete implementation.
 */
struct OrderBookPoolContext {
  ::exchange::core::collections::objpool::ObjectPool<DirectOrder>* orderPool = nullptr;
  ::exchange::core::collections::objpool::ObjectPool<Bucket>* bucketPool = nullptr;
  /** Which ART node pool preset to use (DefaultTest = 4K each; Production/HighLoad = old configs). */
  ArtPoolProfile artPoolProfile = ArtPoolProfile::DefaultTest;
};

}  // namespace exchange::core::orderbook
