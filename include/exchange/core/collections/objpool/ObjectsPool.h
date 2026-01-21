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

#include <type_traits>
#include <unordered_map>
#include <vector>

namespace exchange {
namespace core {
namespace collections {
namespace objpool {

/**
 * ObjectsPool - Object pool for reducing allocations
 * Thread-safe object pool implementation
 */
class ObjectsPool {
public:
  // Pool type constants
  static constexpr int ORDER = 0;
  static constexpr int DIRECT_ORDER = 1;
  static constexpr int DIRECT_BUCKET = 2;
  static constexpr int ART_NODE_4 = 8;
  static constexpr int ART_NODE_16 = 9;
  static constexpr int ART_NODE_48 = 10;
  static constexpr int ART_NODE_256 = 11;
  static constexpr int SYMBOL_POSITION_RECORD = 12;

  /**
   * Create default test pool
   * Small capacity for fast startup, suitable for unit tests and benchmarks
   */
  static ObjectsPool* CreateDefaultTestPool();

  /**
   * Create production pool
   * Large capacity to minimize allocations, suitable for production
   * environments Matches Java MatchingEngineRouter configuration
   */
  static ObjectsPool* CreateProductionPool();

  /**
   * Create high-load pool
   * Extra large capacity for maximum performance, suitable for high-frequency
   * trading
   */
  static ObjectsPool* CreateHighLoadPool();

  /**
   * Constructor with size configuration
   * @param sizesConfig Map of pool type -> size
   */
  explicit ObjectsPool(const std::unordered_map<int, int>& sizesConfig);

  /**
   * Destructor
   */
  ~ObjectsPool();

  // Non-copyable, movable
  ObjectsPool(const ObjectsPool&) = delete;
  ObjectsPool& operator=(const ObjectsPool&) = delete;
  ObjectsPool(ObjectsPool&& other) noexcept;
  ObjectsPool& operator=(ObjectsPool&& other) noexcept;

  /**
   * Get object from pool or create new one
   * @param type Pool type
   * @param supplier Function to create new object if pool is empty
   * @return Object from pool or newly created
   *
   * If object is retrieved from pool and type has default constructor,
   * placement new is used to reconstruct it. This ensures object state is
   * clean.
   *
   * For types without default constructor (e.g., ArtNode4), they should use
   * Init methods to reset state after retrieval (e.g., InitFirstKey).
   *
   * Example:
   *   auto *obj = pool->Get<DirectOrder>(DIRECT_ORDER,
   *       []() { return new DirectOrder(); });
   */
  template <typename T, typename Supplier>
  T* Get(int type, Supplier supplier) {
    T* obj = static_cast<T*>(Pop(type));
    if (obj == nullptr) {
      return supplier();
    }
    // Use placement new only if type has default constructor
    if constexpr (std::is_default_constructible_v<T>) {
      new (obj) T();
    }
    // For types without default constructor (e.g., ArtNode4), rely on Init
    // methods
    return obj;
  }

  /**
   * Put object back to pool
   * @param type Pool type
   * @param object Object to return
   *
   * Destructor is called before putting object back to pool.
   */
  template <typename T>
  void Put(int type, T* object) {
    if (object != nullptr) {
      // Call destructor before returning to pool
      object->~T();
      PutRaw(type, static_cast<void*>(object));
    }
  }

  /**
   * Put raw pointer back to pool (without calling destructor)
   * Use this only if you've already called destructor manually
   */
  void PutRaw(int type, void* object);

private:
  class ArrayStack {
  public:
    explicit ArrayStack(int fixedSize);
    ~ArrayStack();

    void* Pop();
    void Add(void* element);

  private:
    int count_;
    void** objects_;
    int capacity_;
  };

  std::vector<ArrayStack*> pools_;

  void* Pop(int type);
};

}  // namespace objpool
}  // namespace collections
}  // namespace core
}  // namespace exchange
