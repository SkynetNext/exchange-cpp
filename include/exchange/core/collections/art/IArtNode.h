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

#include <cstdint>
#include <list>
#include <string>

namespace exchange {
namespace core {
namespace collections {
namespace objpool {
class ObjectsPool;
} // namespace objpool
namespace art {

// Forward declarations
template <typename V> class LongObjConsumer;

/**
 * IArtNode - interface for ART tree nodes
 * @tparam V Value type
 */
template <typename V> class IArtNode {
public:
  virtual ~IArtNode() = default;

  /**
   * Get value by key
   * @param key 64-bit key
   * @param level Current level (bits from MSB, 56 = initial level)
   * @return Value if found, nullptr otherwise
   */
  virtual V *GetValue(int64_t key, int level) = 0;

  /**
   * Put key-value pair
   * @param key 64-bit key
   * @param level Current level
   * @param value Value to store
   * @return New node if resized (upsized), nullptr otherwise
   */
  virtual IArtNode<V> *Put(int64_t key, int level, V *value) = 0;

  /**
   * Remove key-value pair
   * @param key 64-bit key
   * @param level Current level
   * @return New node if resized (downsized), nullptr if empty, this if
   * unchanged
   */
  virtual IArtNode<V> *Remove(int64_t key, int level) = 0;

  /**
   * Get ceiling value (smallest key >= given key)
   * @param key Lower bound key
   * @param level Current level
   * @return Value with smallest key >= key, or nullptr
   */
  virtual V *GetCeilingValue(int64_t key, int level) = 0;

  /**
   * Get floor value (largest key <= given key)
   * @param key Upper bound key
   * @param level Current level
   * @return Value with largest key <= key, or nullptr
   */
  virtual V *GetFloorValue(int64_t key, int level) = 0;

  /**
   * For each element in ascending order
   * Const method - only reads tree structure, does not modify it
   * @param consumer Consumer function (key, value)
   * @param limit Maximum number of elements to process
   * @return Number of elements processed
   */
  virtual int ForEach(LongObjConsumer<V> *consumer, int limit) const = 0;

  /**
   * For each element in descending order
   * Const method - only reads tree structure, does not modify it
   * @param consumer Consumer function (key, value)
   * @param limit Maximum number of elements to process
   * @return Number of elements processed
   */
  virtual int ForEachDesc(LongObjConsumer<V> *consumer, int limit) const = 0;

  /**
   * Get number of elements
   * @param limit Maximum count to check (for performance)
   * @return Actual size if <= limit, or limit if larger
   */
  virtual int Size(int limit) = 0;

  /**
   * Validate internal state (for testing)
   * @param level Expected level
   */
  virtual void ValidateInternalState(int level) = 0;

  /**
   * Print diagram (for debugging)
   * @param prefix Prefix string
   * @param level Current level
   * @return Diagram string
   */
  virtual std::string PrintDiagram(const std::string &prefix, int level) = 0;

  /**
   * Get entries list (for testing)
   * @return List of key-value pairs
   */
  virtual std::list<std::pair<int64_t, V *>> Entries() = 0;

  /**
   * Get objects pool
   * @return ObjectsPool instance
   */
  virtual objpool::ObjectsPool *GetObjectsPool() = 0;

  /**
   * Get node type for object pool recycling
   * @return Node type constant (ObjectsPool::ART_NODE_4/16/48/256)
   * Non-virtual for performance - returns stored nodeType_ member
   */
  int GetNodeType() const { return nodeType_; }

protected:
  /**
   * Node type for object pool recycling
   * Set by derived classes in their constructors
   */
  int nodeType_;

  /**
   * Protected constructor - derived classes must set nodeType_
   */
  explicit IArtNode(int nodeType) : nodeType_(nodeType) {}
};

} // namespace art
} // namespace collections
} // namespace core
} // namespace exchange
