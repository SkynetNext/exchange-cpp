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

#include <exchange/core/collections/objpool/ObjectsPool.h>
#include <algorithm>
#include <cassert>
#include <unordered_map>

namespace exchange::core::collections::objpool {

ObjectsPool* ObjectsPool::CreateDefaultTestPool() {
  std::unordered_map<int, int> config;
  // Increased capacities for tests that create many objects:
  // - SequentialAsksTest creates ~2000 orders
  // - MultipleCommandsCompareTest processes 100,000 transactions (needs large DIRECT_ORDER pool)
  //   Peak usage can be very high as orders are created, matched, and removed throughout the test
  //   Under TSan, object reuse is slower, requiring even larger capacity
  // - ShouldLoadManyItems creates 100,000 ART entries (needs many nodes)
  //   During insertion, nodes split frequently, requiring more nodes than final count
  // Note: Objects are reused during test execution, but peak usage can exceed these values
  // TSan instrumentation slows down object reuse, so we need extra capacity
  config[DIRECT_ORDER] = 262144;  // Increased from 131072 (for 100K transaction test with TSan
                                  // overhead and high peak usage during stress tests)
  config[DIRECT_BUCKET] = 8192;   // Increased from 4096
  config[ART_NODE_4] = 262144;    // Increased from 131072 (for 100K ART items with splits)
  config[ART_NODE_16] = 131072;   // Increased from 65536
  config[ART_NODE_48] = 65536;    // Increased from 32768
  config[ART_NODE_256] = 32768;   // Increased from 16384
  return new ObjectsPool(config);
}

ObjectsPool* ObjectsPool::CreateProductionPool() {
  std::unordered_map<int, int> config;
  // Production configuration matching Java MatchingEngineRouter
  // Optimized for order book operations with large capacity to minimize
  // allocations
  config[DIRECT_ORDER] = 1024 * 1024;  // 1M orders
  config[DIRECT_BUCKET] = 1024 * 64;   // 64K buckets
  config[ART_NODE_4] = 1024 * 32;      // 32K nodes
  config[ART_NODE_16] = 1024 * 16;     // 16K nodes
  config[ART_NODE_48] = 1024 * 8;      // 8K nodes
  config[ART_NODE_256] = 1024 * 4;     // 4K nodes
  return new ObjectsPool(config);
}

ObjectsPool* ObjectsPool::CreateHighLoadPool() {
  std::unordered_map<int, int> config;
  // High-load configuration for maximum performance
  // Extra large capacity for high-frequency trading scenarios
  config[DIRECT_ORDER] = 1024 * 1024 * 2;  // 2M orders
  config[DIRECT_BUCKET] = 1024 * 128;      // 128K buckets
  config[ART_NODE_4] = 1024 * 64;          // 64K nodes
  config[ART_NODE_16] = 1024 * 32;         // 32K nodes
  config[ART_NODE_48] = 1024 * 16;         // 16K nodes
  config[ART_NODE_256] = 1024 * 8;         // 8K nodes
  return new ObjectsPool(config);
}

ObjectsPool::ObjectsPool(const std::unordered_map<int, int>& sizesConfig) {
  int maxStack = 0;
  for (const auto& entry : sizesConfig) {
    maxStack = std::max(maxStack, entry.first);
  }
  pools_.resize(maxStack + 1, nullptr);
  for (const auto& entry : sizesConfig) {
    pools_[entry.first] = new ArrayStack(entry.second);
  }
}

ObjectsPool::~ObjectsPool() {
  for (ArrayStack* pool : pools_) {
    delete pool;
  }
}

void ObjectsPool::PutRaw(int type, void* object) {
  if (type >= 0 && type < static_cast<int>(pools_.size()) && pools_[type] != nullptr) {
    pools_[type]->Add(object);
  }
}

void* ObjectsPool::Pop(int type) {
  if (type >= 0 && type < static_cast<int>(pools_.size()) && pools_[type] != nullptr) {
    return pools_[type]->Pop();
  }
  return nullptr;
}

// ArrayStack implementation
ObjectsPool::ArrayStack::ArrayStack(int fixedSize)
  : count_(0), capacity_(fixedSize), objects_(new void*[fixedSize]) {
  for (int i = 0; i < capacity_; i++) {
    // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
    objects_[i] = nullptr;
  }
}

ObjectsPool::ArrayStack::~ArrayStack() {
  delete[] objects_;
}

void ObjectsPool::ArrayStack::Add(void* element) {
#ifndef NDEBUG
  // 调试模式下断言，帮助发现池容量不足的问题
  if (count_ == capacity_) {
    assert(false && "ObjectsPool: Pool is full, consider increasing capacity!");
  }
#endif
  if (count_ != capacity_) {
    // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
    objects_[count_] = element;
    count_++;
  }
}

void* ObjectsPool::ArrayStack::Pop() {
  if (count_ != 0) {
    count_--;
    // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
    void* object = objects_[count_];
    // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
    objects_[count_] = nullptr;
    return object;
  }
  return nullptr;
}

}  // namespace exchange::core::collections::objpool
