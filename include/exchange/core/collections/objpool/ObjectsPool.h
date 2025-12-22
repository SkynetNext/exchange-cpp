/*
 * Copyright 2019-2020 Maksim Zheravin
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
   */
  static ObjectsPool *CreateDefaultTestPool();

  /**
   * Constructor with size configuration
   * @param sizesConfig Map of pool type -> size
   */
  explicit ObjectsPool(const std::unordered_map<int, int> &sizesConfig);

  /**
   * Destructor
   */
  ~ObjectsPool();

  // Non-copyable, movable
  ObjectsPool(const ObjectsPool &) = delete;
  ObjectsPool &operator=(const ObjectsPool &) = delete;
  ObjectsPool(ObjectsPool &&other) noexcept;
  ObjectsPool &operator=(ObjectsPool &&other) noexcept;

  /**
   * Get object from pool or create new one
   * @param type Pool type
   * @param supplier Function to create new object if pool is empty
   * @return Object from pool or newly created
   */
  template <typename T, typename Supplier> T *Get(int type, Supplier supplier) {
    T *obj = static_cast<T *>(Pop(type));
    if (obj == nullptr) {
      return supplier();
    }
    return obj;
  }

  /**
   * Put object back to pool
   * @param type Pool type
   * @param object Object to return
   */
  void Put(int type, void *object);

private:
  class ArrayStack {
  public:
    explicit ArrayStack(int fixedSize);
    ~ArrayStack();

    void *Pop();
    void Add(void *element);

  private:
    int count_;
    void **objects_;
    int capacity_;
  };

  std::vector<ArrayStack *> pools_;

  void *Pop(int type);
};

} // namespace objpool
} // namespace collections
} // namespace core
} // namespace exchange
