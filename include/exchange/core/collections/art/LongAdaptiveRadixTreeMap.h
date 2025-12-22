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

#include "IArtNode.h"
#include "LongObjConsumer.h"
#include <cstdint>
#include <functional>
#include <list>

namespace exchange {
namespace core {
namespace collections {
namespace art {

// Forward declarations
namespace objpool {
class ObjectsPool;
}

/**
 * LongAdaptiveRadixTreeMap - Adaptive Radix Tree (ART) for 64-bit keys
 *
 * Based on original paper:
 * The Adaptive Radix Tree: ARTful Indexing for Main-Memory Databases
 * Viktor Leis, Alfons Kemper, Thomas Neumann
 * https://db.in.tum.de/~leis/papers/ART.pdf
 *
 * Target operations:
 * - GET or (PUT + GET_LOWER/HIGHER) - placing/moving/bulkload order - often
 * GET, more rare PUT
 * - REMOVE - cancel or move - last order in the bucket
 * - TRAVERSE from LOWER - filling L2 market data, in hot area (Node256 or
 * Node48)
 * - REMOVE price during matching - can use RANGE removal operation - rare, but
 * latency critical
 * - GET or PUT if not exists - inserting back own orders, very rare
 *
 * @tparam V Value type
 */
template <typename V> class LongAdaptiveRadixTreeMap {
public:
  static constexpr int INITIAL_LEVEL = 56;

  /**
   * Constructor with object pool
   * @param objectsPool Object pool for node allocation
   */
  explicit LongAdaptiveRadixTreeMap(
      ::exchange::core::collections::objpool::ObjectsPool *objectsPool);

  /**
   * Default constructor (creates default test pool)
   */
  LongAdaptiveRadixTreeMap();

  /**
   * Destructor
   */
  ~LongAdaptiveRadixTreeMap();

  // Non-copyable, movable
  LongAdaptiveRadixTreeMap(const LongAdaptiveRadixTreeMap &) = delete;
  LongAdaptiveRadixTreeMap &
  operator=(const LongAdaptiveRadixTreeMap &) = delete;
  LongAdaptiveRadixTreeMap(LongAdaptiveRadixTreeMap &&other) noexcept;
  LongAdaptiveRadixTreeMap &
  operator=(LongAdaptiveRadixTreeMap &&other) noexcept;

  /**
   * Get value by key
   * @param key 64-bit key
   * @return Value pointer if found, nullptr otherwise
   */
  V *Get(int64_t key) const;

  /**
   * Put key-value pair
   * @param key 64-bit key
   * @param value Value to store
   */
  void Put(int64_t key, V *value);

  /**
   * Get or insert (if not exists, create using supplier)
   * @param key 64-bit key
   * @param supplier Function to create value if not exists
   * @return Existing or newly created value
   */
  V *GetOrInsert(int64_t key, std::function<V *()> supplier);

  /**
   * Remove key-value pair
   * @param key 64-bit key
   */
  void Remove(int64_t key);

  /**
   * Clear all elements
   */
  void Clear();

  /**
   * Remove keys in range [keyFromInclusive, keyToExclusive)
   * @param keyFromInclusive Start key (inclusive)
   * @param keyToExclusive End key (exclusive)
   */
  void RemoveRange(int64_t keyFromInclusive, int64_t keyToExclusive);

  /**
   * Get higher value (smallest key > given key)
   * Equivalent to lowerBound(key + 1)
   * @param key Key to search from
   * @return Value with smallest key > key, or nullptr
   */
  V *GetHigherValue(int64_t key) const;

  /**
   * Get lower value (largest key < given key)
   * Equivalent to upperBound(key - 1)
   * @param key Key to search from
   * @return Value with largest key < key, or nullptr
   */
  V *GetLowerValue(int64_t key) const;

  /**
   * For each element in ascending order
   * @param consumer Consumer function
   * @param limit Maximum number of elements to process
   * @return Number of elements processed
   */
  int ForEach(LongObjConsumer<V> *consumer, int limit);

  /**
   * For each element in descending order
   * @param consumer Consumer function
   * @param limit Maximum number of elements to process
   * @return Number of elements processed
   */
  int ForEachDesc(LongObjConsumer<V> *consumer, int limit);

  /**
   * Get size (up to limit for performance)
   * @param limit Maximum count to check
   * @return Actual size if <= limit, or limit if larger
   */
  int Size(int limit) const;

  /**
   * Get entries list (for testing)
   * @return List of key-value pairs
   */
  std::list<std::pair<int64_t, V *>> EntriesList() const;

  /**
   * Validate internal state (for testing)
   */
  void ValidateInternalState() const;

  /**
   * Print diagram (for debugging)
   * @return Diagram string
   */
  std::string PrintDiagram() const;

  /**
   * Entry class for key-value pairs
   */
  class Entry {
  public:
    int64_t key;
    V *value;

    Entry(int64_t k, V *v) : key(k), value(v) {}
  };

  // Helper method for branching (public so ArtNode classes can use it)
  static IArtNode<V> *BranchIfRequired(int64_t key, V *value, int64_t nodeKey,
                                       int nodeLevel, IArtNode<V> *caller);

  // Helper method for printing diagram (used by all node types)
  static std::string PrintDiagram(const std::string &prefix, int level,
                                  int nodeLevel, int64_t nodeKey,
                                  int numChildren,
                                  std::function<int16_t(int)> getSubKey,
                                  std::function<void *(int)> getNode);

  // Helper method to recycle old node to pool
  static void RecycleNodeToPool(IArtNode<V> *oldNode);

private:
  // Root node
  IArtNode<V> *root_;

  // Object pool
  ::exchange::core::collections::objpool::ObjectsPool *objectsPool_;
};

} // namespace art
} // namespace collections
} // namespace core
} // namespace exchange
