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

#include <algorithm>
#include <climits>
#include <exchange/core/collections/art/ArtNode4.h>
#include <exchange/core/collections/art/LongAdaptiveRadixTreeMap.h>
#include <exchange/core/collections/objpool/ObjectsPool.h>
#include <stdexcept>

namespace exchange {
namespace core {
namespace collections {
namespace art {

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
  // Note: We don't delete objectsPool_ here as it may be shared
  // The caller is responsible for managing the ObjectsPool lifetime
}

template <typename V>
LongAdaptiveRadixTreeMap<V>::LongAdaptiveRadixTreeMap(
    LongAdaptiveRadixTreeMap &&other) noexcept
    : root_(other.root_), objectsPool_(other.objectsPool_) {
  other.root_ = nullptr;
  other.objectsPool_ = nullptr;
}

template <typename V>
LongAdaptiveRadixTreeMap<V> &LongAdaptiveRadixTreeMap<V>::operator=(
    LongAdaptiveRadixTreeMap &&other) noexcept {
  if (this != &other) {
    Clear();
    root_ = other.root_;
    objectsPool_ = other.objectsPool_;
    other.root_ = nullptr;
    other.objectsPool_ = nullptr;
  }
  return *this;
}

template <typename V> V *LongAdaptiveRadixTreeMap<V>::Get(int64_t key) const {
  return root_ != nullptr ? root_->GetValue(key, INITIAL_LEVEL) : nullptr;
}

template <typename V>
void LongAdaptiveRadixTreeMap<V>::Put(int64_t key, V *value) {
  if (root_ == nullptr) {
    auto *node = objectsPool_->Get(
        ::exchange::core::collections::objpool::ObjectsPool::ART_NODE_4,
        [this]() { return new ArtNode4<V>(objectsPool_); });
    node->InitFirstKey(key, value);
    root_ = node;
  } else {
    IArtNode<V> *upSizedNode = root_->Put(key, INITIAL_LEVEL, value);
    if (upSizedNode != nullptr) {
      // TODO: Put old node into pool
      root_ = upSizedNode;
    }
  }
}

template <typename V>
V *LongAdaptiveRadixTreeMap<V>::GetOrInsert(int64_t key,
                                            std::function<V *()> supplier) {
  // TODO: Implement
  V *value = Get(key);
  if (value == nullptr) {
    value = supplier();
    Put(key, value);
  }
  return value;
}

template <typename V> void LongAdaptiveRadixTreeMap<V>::Remove(int64_t key) {
  if (root_ != nullptr) {
    IArtNode<V> *downSizeNode = root_->Remove(key, INITIAL_LEVEL);
    // Ignore null because can not remove root
    if (downSizeNode != root_) {
      // TODO: Put old node into pool
      root_ = downSizeNode;
    }
  }
}

template <typename V> void LongAdaptiveRadixTreeMap<V>::Clear() {
  // Produces garbage (doesn't return nodes to pool)
  root_ = nullptr;
}

template <typename V>
void LongAdaptiveRadixTreeMap<V>::RemoveRange(int64_t keyFromInclusive,
                                              int64_t keyToExclusive) {
  // TODO: Implement
  throw std::runtime_error("RemoveRange not implemented");
}

template <typename V>
V *LongAdaptiveRadixTreeMap<V>::GetHigherValue(int64_t key) const {
  if (root_ != nullptr && key != INT64_MAX) {
    return root_->GetCeilingValue(key + 1, INITIAL_LEVEL);
  } else {
    return nullptr;
  }
}

template <typename V>
V *LongAdaptiveRadixTreeMap<V>::GetLowerValue(int64_t key) const {
  if (root_ != nullptr && key != 0) {
    return root_->GetFloorValue(key - 1, INITIAL_LEVEL);
  } else {
    return nullptr;
  }
}

template <typename V>
int LongAdaptiveRadixTreeMap<V>::ForEach(LongObjConsumer<V> *consumer,
                                         int limit) {
  if (root_ != nullptr) {
    return root_->ForEach(consumer, limit);
  } else {
    return 0;
  }
}

template <typename V>
int LongAdaptiveRadixTreeMap<V>::ForEachDesc(LongObjConsumer<V> *consumer,
                                             int limit) {
  if (root_ != nullptr) {
    return root_->ForEachDesc(consumer, limit);
  } else {
    return 0;
  }
}

template <typename V> int LongAdaptiveRadixTreeMap<V>::Size(int limit) const {
  if (root_ != nullptr) {
    return std::min(root_->Size(limit), limit);
  } else {
    return 0;
  }
}

template <typename V>
std::list<std::pair<int64_t, V *>>
LongAdaptiveRadixTreeMap<V>::EntriesList() const {
  if (root_ != nullptr) {
    return root_->Entries();
  } else {
    return std::list<std::pair<int64_t, V *>>();
  }
}

template <typename V>
void LongAdaptiveRadixTreeMap<V>::ValidateInternalState() const {
  if (root_ != nullptr) {
    root_->ValidateInternalState(INITIAL_LEVEL);
  }
}

template <typename V>
std::string LongAdaptiveRadixTreeMap<V>::PrintDiagram() const {
  if (root_ != nullptr) {
    return root_->PrintDiagram("", INITIAL_LEVEL);
  } else {
    return "";
  }
}

template <typename V>
IArtNode<V> *
LongAdaptiveRadixTreeMap<V>::BranchIfRequired(int64_t key, V *value,
                                              int64_t nodeKey, int nodeLevel,
                                              IArtNode<V> *caller) {

  const int64_t keyDiff = key ^ nodeKey;

  // Check if there is common part
  if ((keyDiff & (-1LL << (nodeLevel + 8))) == 0) {
    return nullptr;
  }

  // On which level
  const int newLevel = (63 - __builtin_clzll(keyDiff)) & 0xF8;
  if (newLevel == nodeLevel) {
    return nullptr;
  }

  ::exchange::core::collections::objpool::ObjectsPool *objectsPool =
      caller->GetObjectsPool();
  auto *newSubNode = objectsPool->Get(
      ::exchange::core::collections::objpool::ObjectsPool::ART_NODE_4,
      [objectsPool]() { return new ArtNode4<V>(objectsPool); });
  newSubNode->InitFirstKey(key, value);

  auto *newNode = objectsPool->Get(
      ::exchange::core::collections::objpool::ObjectsPool::ART_NODE_4,
      [objectsPool]() { return new ArtNode4<V>(objectsPool); });
  newNode->InitTwoKeys(nodeKey, caller, key, newSubNode, newLevel);

  return newNode;
}

// Explicit template instantiations for common types
// These will be used in OrderBookDirectImpl
template class LongAdaptiveRadixTreeMap<void>; // For Bucket* and DirectOrder*

} // namespace art
} // namespace collections
} // namespace core
} // namespace exchange
