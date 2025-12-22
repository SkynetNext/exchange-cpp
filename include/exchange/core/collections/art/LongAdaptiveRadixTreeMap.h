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
#include <algorithm>
#include <cstdint>
#include <functional>
#include <iomanip>
#include <list>
#include <sstream>
#include <stdexcept>
#include <string>

namespace exchange {
namespace core {
namespace collections {

namespace objpool {
class ObjectsPool;
}

namespace art {

// Forward declarations
template <typename V> class IArtNode;
template <typename V> class ArtNode4;
template <typename V> class ArtNode16;
template <typename V> class ArtNode48;
template <typename V> class ArtNode256;

/**
 * Branching utility function (moved out of class to avoid circular dependency)
 */
template <typename V>
IArtNode<V> *BranchIfRequired(int64_t key, V *value, int64_t nodeKey,
                              int nodeLevel, IArtNode<V> *caller);

/**
 * Node recycling utility
 */
template <typename V> void RecycleNodeToPool(IArtNode<V> *oldNode);

/**
 * LongAdaptiveRadixTreeMap - ART tree implementation for 64-bit long keys
 *
 * @tparam V Value type
 */
template <typename V> class LongAdaptiveRadixTreeMap {
public:
  static constexpr int INITIAL_LEVEL = 56;

  explicit LongAdaptiveRadixTreeMap(
      ::exchange::core::collections::objpool::ObjectsPool *objectsPool);
  LongAdaptiveRadixTreeMap();
  ~LongAdaptiveRadixTreeMap();

  LongAdaptiveRadixTreeMap(const LongAdaptiveRadixTreeMap &) = delete;
  LongAdaptiveRadixTreeMap &
  operator=(const LongAdaptiveRadixTreeMap &) = delete;
  LongAdaptiveRadixTreeMap(LongAdaptiveRadixTreeMap &&) noexcept;
  LongAdaptiveRadixTreeMap &operator=(LongAdaptiveRadixTreeMap &&) noexcept;

  V *Get(int64_t key) const;
  void Put(int64_t key, V *value);
  V *GetOrInsert(int64_t key, std::function<V *()> supplier);
  void Remove(int64_t key);
  void Clear();
  void RemoveRange(int64_t keyFromInclusive, int64_t keyToExclusive);

  V *GetHigherValue(int64_t key) const;
  V *GetLowerValue(int64_t key) const;

  int ForEach(LongObjConsumer<V> *consumer, int limit);
  int ForEachDesc(LongObjConsumer<V> *consumer, int limit);

  int Size(int limit) const;
  std::list<std::pair<int64_t, V *>> EntriesList() const;
  void ValidateInternalState() const;
  std::string PrintDiagram() const;

  static std::string PrintDiagram(const std::string &prefix, int level,
                                  int nodeLevel, int64_t nodeKey,
                                  int numChildren,
                                  std::function<int16_t(int)> getSubKey,
                                  std::function<void *(int)> getNode);

private:
  IArtNode<V> *root_;
  ::exchange::core::collections::objpool::ObjectsPool *objectsPool_;
};

} // namespace art
} // namespace collections
} // namespace core
} // namespace exchange

// Include implementations
#include "ArtNode16.h"
#include "ArtNode256.h"
#include "ArtNode4.h"
#include "ArtNode48.h"

namespace exchange {
namespace core {
namespace collections {
namespace art {

// --- Global Utilities Implementation ---

template <typename V>
IArtNode<V> *BranchIfRequired(int64_t key, V *value, int64_t nodeKey,
                              int nodeLevel, IArtNode<V> *caller) {
  const int64_t keyDiff = key ^ nodeKey;
  if ((keyDiff & (-1LL << nodeLevel)) == 0) {
    return nullptr;
  }
  const int newLevel = (63 - __builtin_clzll(keyDiff)) & 0xF8;
  if (newLevel == nodeLevel) {
    return nullptr;
  }
  auto *pool = caller->GetObjectsPool();
  auto *newSubNode = pool->template Get<ArtNode4<V>>(
      ::exchange::core::collections::objpool::ObjectsPool::ART_NODE_4,
      [pool]() { return new ArtNode4<V>(pool); });
  newSubNode->InitFirstKey(key, value);
  auto *newNode = pool->template Get<ArtNode4<V>>(
      ::exchange::core::collections::objpool::ObjectsPool::ART_NODE_4,
      [pool]() { return new ArtNode4<V>(pool); });
  newNode->InitTwoKeys(nodeKey, caller, key, newSubNode, newLevel);
  return newNode;
}

template <typename V> void RecycleNodeToPool(IArtNode<V> *oldNode) {
  if (oldNode == nullptr)
    return;
  auto *pool = oldNode->GetObjectsPool();
  if (pool == nullptr)
    return;
  if (dynamic_cast<ArtNode4<V> *>(oldNode)) {
    pool->Put(::exchange::core::collections::objpool::ObjectsPool::ART_NODE_4,
              oldNode);
  } else if (dynamic_cast<ArtNode16<V> *>(oldNode)) {
    pool->Put(::exchange::core::collections::objpool::ObjectsPool::ART_NODE_16,
              oldNode);
  } else if (dynamic_cast<ArtNode48<V> *>(oldNode)) {
    pool->Put(::exchange::core::collections::objpool::ObjectsPool::ART_NODE_48,
              oldNode);
  } else if (dynamic_cast<ArtNode256<V> *>(oldNode)) {
    pool->Put(::exchange::core::collections::objpool::ObjectsPool::ART_NODE_256,
              oldNode);
  }
}

// --- LongAdaptiveRadixTreeMap Implementation ---

template <typename V>
LongAdaptiveRadixTreeMap<V>::LongAdaptiveRadixTreeMap(
    ::exchange::core::collections::objpool::ObjectsPool *objectsPool)
    : root_(nullptr), objectsPool_(objectsPool) {
  if (objectsPool_ == nullptr) {
    objectsPool_ = ::exchange::core::collections::objpool::ObjectsPool::
        CreateDefaultTestPool();
  }
}

template <typename V>
LongAdaptiveRadixTreeMap<V>::LongAdaptiveRadixTreeMap()
    : root_(nullptr), objectsPool_(::exchange::core::collections::objpool::
                                       ObjectsPool::CreateDefaultTestPool()) {}

template <typename V> LongAdaptiveRadixTreeMap<V>::~LongAdaptiveRadixTreeMap() {
  Clear();
}

template <typename V>
LongAdaptiveRadixTreeMap<V>::LongAdaptiveRadixTreeMap(
    LongAdaptiveRadixTreeMap &&other) noexcept
    : root_(other.root_), objectsPool_(other.objectsPool_) {
  other.root_ = nullptr;
}

template <typename V>
LongAdaptiveRadixTreeMap<V> &LongAdaptiveRadixTreeMap<V>::operator=(
    LongAdaptiveRadixTreeMap &&other) noexcept {
  if (this != &other) {
    Clear();
    root_ = other.root_;
    objectsPool_ = other.objectsPool_;
    other.root_ = nullptr;
  }
  return *this;
}

template <typename V> V *LongAdaptiveRadixTreeMap<V>::Get(int64_t key) const {
  return root_ ? root_->GetValue(key, INITIAL_LEVEL) : nullptr;
}

template <typename V>
void LongAdaptiveRadixTreeMap<V>::Put(int64_t key, V *value) {
  if (root_ == nullptr) {
    auto *node = objectsPool_->template Get<ArtNode4<V>>(
        ::exchange::core::collections::objpool::ObjectsPool::ART_NODE_4,
        [this]() { return new ArtNode4<V>(objectsPool_); });
    node->InitFirstKey(key, value);
    root_ = node;
  } else {
    IArtNode<V> *upSizedNode = root_->Put(key, INITIAL_LEVEL, value);
    if (upSizedNode != nullptr) {
      root_ = upSizedNode;
    }
  }
}

template <typename V>
V *LongAdaptiveRadixTreeMap<V>::GetOrInsert(int64_t key,
                                            std::function<V *()> supplier) {
  V *v = Get(key);
  if (!v) {
    v = supplier();
    Put(key, v);
  }
  return v;
}

template <typename V> void LongAdaptiveRadixTreeMap<V>::Remove(int64_t key) {
  if (root_) {
    IArtNode<V> *downSizeNode = root_->Remove(key, INITIAL_LEVEL);
    if (downSizeNode != root_) {
      root_ = downSizeNode;
    }
  }
}

template <typename V> void LongAdaptiveRadixTreeMap<V>::Clear() {
  root_ = nullptr;
}

template <typename V>
void LongAdaptiveRadixTreeMap<V>::RemoveRange(int64_t, int64_t) {
  throw std::runtime_error("RemoveRange not implemented");
}

template <typename V>
V *LongAdaptiveRadixTreeMap<V>::GetHigherValue(int64_t key) const {
  return (root_ && key != INT64_MAX)
             ? root_->GetCeilingValue(key + 1, INITIAL_LEVEL)
             : nullptr;
}

template <typename V>
V *LongAdaptiveRadixTreeMap<V>::GetLowerValue(int64_t key) const {
  return (root_ && key != 0) ? root_->GetFloorValue(key - 1, INITIAL_LEVEL)
                             : nullptr;
}

template <typename V>
int LongAdaptiveRadixTreeMap<V>::ForEach(LongObjConsumer<V> *consumer,
                                         int limit) {
  return root_ ? root_->ForEach(consumer, limit) : 0;
}

template <typename V>
int LongAdaptiveRadixTreeMap<V>::ForEachDesc(LongObjConsumer<V> *consumer,
                                             int limit) {
  return root_ ? root_->ForEachDesc(consumer, limit) : 0;
}

template <typename V> int LongAdaptiveRadixTreeMap<V>::Size(int limit) const {
  return root_ ? std::min(root_->Size(limit), limit) : 0;
}

template <typename V>
std::list<std::pair<int64_t, V *>>
LongAdaptiveRadixTreeMap<V>::EntriesList() const {
  return root_ ? root_->Entries() : std::list<std::pair<int64_t, V *>>();
}

template <typename V>
void LongAdaptiveRadixTreeMap<V>::ValidateInternalState() const {
  if (root_)
    root_->ValidateInternalState(INITIAL_LEVEL);
}

template <typename V>
std::string LongAdaptiveRadixTreeMap<V>::PrintDiagram() const {
  return root_ ? root_->PrintDiagram("", INITIAL_LEVEL) : "";
}

template <typename V>
std::string LongAdaptiveRadixTreeMap<V>::PrintDiagram(
    const std::string &prefix, int level, int nodeLevel, int64_t nodeKey,
    int numChildren, std::function<int16_t(int)> getSubKey,
    std::function<void *(int)> getNode) {
  std::string baseKeyPrefix, baseKeyPrefix1;
  const int lvlDiff = level - nodeLevel;
  if (lvlDiff != 0) {
    int chars = lvlDiff >> 2;
    int64_t mask = ((1LL << lvlDiff) - 1LL);
    int64_t keyPart = (nodeKey >> (nodeLevel + 8)) & mask;
    std::ostringstream oss;
    for (int j = 0; j < chars - 2; j++)
      oss << "─";
    oss << "[" << std::hex << std::uppercase << std::setfill('0')
        << std::setw(chars) << keyPart << "]";
    baseKeyPrefix = oss.str();
    baseKeyPrefix1 = std::string(chars * 2, ' ');
  }
  std::ostringstream sb;
  for (int i = 0; i < numChildren; i++) {
    void *node = getNode(i);
    int16_t subKey = getSubKey(i);
    std::ostringstream keyStream;
    keyStream << baseKeyPrefix << std::hex << std::uppercase
              << std::setfill('0') << std::setw(2) << subKey;
    std::string key = keyStream.str();
    std::string x = (i == 0) ? (numChildren == 1 ? "──" : "┬─")
                             : (prefix + (i + 1 == numChildren ? "└─" : "├─"));
    if (nodeLevel == 0) {
      sb << x << key << " = " << node;
    } else {
      std::string nextPrefix =
          prefix + (i + 1 == numChildren ? "    " : "│   ") + baseKeyPrefix1;
      sb << x << key
         << static_cast<IArtNode<V> *>(node)->PrintDiagram(nextPrefix,
                                                           nodeLevel - 8);
    }
    if (i < numChildren - 1)
      sb << "\n";
    else if (nodeLevel == 0)
      sb << "\n" << prefix;
  }
  return sb.str();
}

} // namespace art
} // namespace collections
} // namespace core
} // namespace exchange
