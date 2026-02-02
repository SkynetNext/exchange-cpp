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

#include <algorithm>
#include <cstdint>
#include <functional>
#include <iomanip>
#include <list>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <type_traits>
#include "IArtNode.h"
#include "LongObjConsumer.h"

namespace exchange::core::collections::art {

// Forward declarations
template <typename V>
class IArtNode;
template <typename V>
class ArtNode4;
template <typename V>
class ArtNode16;
template <typename V>
class ArtNode48;
template <typename V>
class ArtNode256;
template <typename V>
class ArtPoolContext;

/**
 * Branching utility (caller must pass ArtPoolContext; nodes hold context and pass it)
 */
template <typename V>
IArtNode<V>* BranchIfRequired(ArtPoolContext<V>* ctx,
                              int64_t key,
                              V* value,
                              int64_t nodeKey,
                              int nodeLevel,
                              IArtNode<V>* caller);

/**
 * LongAdaptiveRadixTreeMap - ART tree implementation for 64-bit long keys
 *
 * @tparam V Value type
 */
template <typename V>
class LongAdaptiveRadixTreeMap {
public:
  static constexpr int INITIAL_LEVEL = 56;

  explicit LongAdaptiveRadixTreeMap(ArtPoolContext<V>* poolContext);
  LongAdaptiveRadixTreeMap();
  ~LongAdaptiveRadixTreeMap();

  LongAdaptiveRadixTreeMap(const LongAdaptiveRadixTreeMap&) = delete;
  LongAdaptiveRadixTreeMap& operator=(const LongAdaptiveRadixTreeMap&) = delete;
  LongAdaptiveRadixTreeMap(LongAdaptiveRadixTreeMap&&) noexcept;
  LongAdaptiveRadixTreeMap& operator=(LongAdaptiveRadixTreeMap&&) noexcept;

  V* Get(int64_t key) const;
  void Put(int64_t key, V* value);
  V* GetOrInsert(int64_t key, std::function<V*()> supplier);
  void Remove(int64_t key);
  void Clear();
  void RemoveRange(int64_t keyFromInclusive, int64_t keyToExclusive);

  V* GetHigherValue(int64_t key) const;
  V* GetLowerValue(int64_t key) const;

  // Const methods - only read tree structure, do not modify it
  int ForEach(LongObjConsumer<V>* consumer, int limit) const;
  int ForEachDesc(LongObjConsumer<V>* consumer, int limit) const;

  template <typename F>
    requires(!std::is_convertible_v<F, LongObjConsumer<V>*>)
  int ForEach(F f, int limit) const {
    LambdaConsumer<V, F> consumer(f);
    return ForEach(static_cast<LongObjConsumer<V>*>(&consumer), limit);
  }

  template <typename F>
    requires(!std::is_convertible_v<F, LongObjConsumer<V>*>)
  int ForEachDesc(F f, int limit) const {
    LambdaConsumer<V, F> consumer(f);
    return ForEachDesc(static_cast<LongObjConsumer<V>*>(&consumer), limit);
  }

  int Size(int limit) const;
  std::list<std::pair<int64_t, V*>> EntriesList() const;
  void ValidateInternalState() const;
  std::string PrintDiagram() const;

  static std::string PrintDiagram(const std::string& prefix,
                                  int level,
                                  int nodeLevel,
                                  int64_t nodeKey,
                                  int numChildren,
                                  std::function<uint8_t(int)> getSubKey,
                                  std::function<void*(int)> getNode);

private:
  IArtNode<V>* root_;
  ArtPoolContext<V>* poolContext_{nullptr};
  std::unique_ptr<ArtPoolContext<V>> ownedPoolContext_;
};
}  // namespace exchange::core::collections::art

// Include node headers then ObjectPool, then define ArtPoolContext
#include <exchange/core/collections/objpool/ObjectPool.h>
#include "ArtNode16.h"
#include "ArtNode256.h"
#include "ArtNode4.h"
#include "ArtNode48.h"

namespace exchange::core::collections::art {

/**
 * ArtPoolContext - holds one ObjectPool per ART node type for Acquire/Release
 */
template <typename V>
class ArtPoolContext {
public:
  ArtPoolContext(size_t cap4, size_t cap16, size_t cap48, size_t cap256)
    : pool4_(cap4), pool16_(cap16), pool48_(cap48), pool256_(cap256) {}

  static std::unique_ptr<ArtPoolContext<V>> CreateDefaultTest() {
    constexpr size_t kDefaultCap = 4096;
    return std::make_unique<ArtPoolContext<V>>(kDefaultCap, kDefaultCap, kDefaultCap, kDefaultCap);
  }

  ArtNode4<V>* AcquireNode4() {
    return pool4_.Acquire(this);
  }

  ArtNode16<V>* AcquireNode16() {
    return pool16_.Acquire(this);
  }

  ArtNode48<V>* AcquireNode48() {
    return pool48_.Acquire(this);
  }

  ArtNode256<V>* AcquireNode256() {
    return pool256_.Acquire(this);
  }

  void ReleaseNode(IArtNode<V>* node) {
    if (node == nullptr)
      return;
    switch (node->GetNodeType()) {
      case kArtNode4:
        pool4_.Release(static_cast<ArtNode4<V>*>(node));
        break;
      case kArtNode16:
        pool16_.Release(static_cast<ArtNode16<V>*>(node));
        break;
      case kArtNode48:
        pool48_.Release(static_cast<ArtNode48<V>*>(node));
        break;
      case kArtNode256:
        pool256_.Release(static_cast<ArtNode256<V>*>(node));
        break;
      default:
        break;
    }
  }

private:
  ::exchange::core::collections::objpool::ObjectPool<ArtNode4<V>> pool4_;
  ::exchange::core::collections::objpool::ObjectPool<ArtNode16<V>> pool16_;
  ::exchange::core::collections::objpool::ObjectPool<ArtNode48<V>> pool48_;
  ::exchange::core::collections::objpool::ObjectPool<ArtNode256<V>> pool256_;
};

// --- BranchIfRequired Implementation ---

template <typename V>
IArtNode<V>* BranchIfRequired(ArtPoolContext<V>* ctx,
                              int64_t key,
                              V* value,
                              int64_t nodeKey,
                              int nodeLevel,
                              IArtNode<V>* caller) {
  const int64_t keyDiff = key ^ nodeKey;
  if ((keyDiff & (-1LL << nodeLevel)) == 0) {
    return nullptr;
  }
  const int newLevel = (63 - __builtin_clzll(keyDiff)) & 0xF8;
  if (newLevel == nodeLevel) {
    return nullptr;
  }
  ArtNode4<V>* newSubNode = ctx->AcquireNode4();
  if (newSubNode == nullptr)
    return nullptr;
  newSubNode->InitFirstKey(key, value);
  ArtNode4<V>* newNode = ctx->AcquireNode4();
  if (newNode == nullptr) {
    ctx->ReleaseNode(newSubNode);
    return nullptr;
  }
  newNode->InitTwoKeys(nodeKey, caller, key, newSubNode, newLevel);
  return newNode;
}

// --- LongAdaptiveRadixTreeMap Implementation ---

template <typename V>
LongAdaptiveRadixTreeMap<V>::LongAdaptiveRadixTreeMap(ArtPoolContext<V>* poolContext)
  : root_(nullptr), poolContext_(poolContext) {
  if (poolContext_ == nullptr) {
    ownedPoolContext_ = ArtPoolContext<V>::CreateDefaultTest();
    poolContext_ = ownedPoolContext_.get();
  }
}

template <typename V>
LongAdaptiveRadixTreeMap<V>::LongAdaptiveRadixTreeMap() : root_(nullptr) {
  ownedPoolContext_ = ArtPoolContext<V>::CreateDefaultTest();
  poolContext_ = ownedPoolContext_.get();
}

template <typename V>
LongAdaptiveRadixTreeMap<V>::~LongAdaptiveRadixTreeMap() {
  Clear();
}

template <typename V>
LongAdaptiveRadixTreeMap<V>::LongAdaptiveRadixTreeMap(LongAdaptiveRadixTreeMap&& other) noexcept
  : root_(other.root_)
  , poolContext_(other.poolContext_)
  , ownedPoolContext_(std::move(other.ownedPoolContext_)) {
  other.root_ = nullptr;
  other.poolContext_ = nullptr;
}

template <typename V>
LongAdaptiveRadixTreeMap<V>&
LongAdaptiveRadixTreeMap<V>::operator=(LongAdaptiveRadixTreeMap&& other) noexcept {
  if (this != &other) {
    Clear();
    root_ = other.root_;
    poolContext_ = other.poolContext_;
    ownedPoolContext_ = std::move(other.ownedPoolContext_);
    other.root_ = nullptr;
    other.poolContext_ = nullptr;
  }
  return *this;
}

template <typename V>
V* LongAdaptiveRadixTreeMap<V>::Get(int64_t key) const {
  return root_ ? root_->GetValue(key, INITIAL_LEVEL) : nullptr;
}

template <typename V>
void LongAdaptiveRadixTreeMap<V>::Put(int64_t key, V* value) {
  if (root_ == nullptr) {
    ArtNode4<V>* node = poolContext_->AcquireNode4();
    if (node == nullptr)
      return;
    node->InitFirstKey(key, value);
    root_ = node;
  } else {
    auto [replacement, release_old] = root_->Put(key, INITIAL_LEVEL, value);
    if (replacement != nullptr) {
      if (release_old)
        poolContext_->ReleaseNode(root_);
      root_ = replacement;
    }
  }
}

template <typename V>
V* LongAdaptiveRadixTreeMap<V>::GetOrInsert(int64_t key, std::function<V*()> supplier) {
  V* v = Get(key);
  if (!v) {
    v = supplier();
    Put(key, v);
  }
  return v;
}

template <typename V>
void LongAdaptiveRadixTreeMap<V>::Remove(int64_t key) {
  if (root_) {
    IArtNode<V>* downSizeNode = root_->Remove(key, INITIAL_LEVEL);
    if (downSizeNode != root_) {
      poolContext_->ReleaseNode(root_);
      root_ = downSizeNode;
    }
  }
}

template <typename V>
void LongAdaptiveRadixTreeMap<V>::Clear() {
  if (root_ != nullptr) {
    root_->RecycleTree();
    poolContext_->ReleaseNode(root_);
    root_ = nullptr;
  }
}

template <typename V>
void LongAdaptiveRadixTreeMap<V>::RemoveRange(int64_t, int64_t) {
  throw std::runtime_error("RemoveRange not implemented");
}

template <typename V>
V* LongAdaptiveRadixTreeMap<V>::GetHigherValue(int64_t key) const {
  return (root_ && key != INT64_MAX) ? root_->GetCeilingValue(key + 1, INITIAL_LEVEL) : nullptr;
}

template <typename V>
V* LongAdaptiveRadixTreeMap<V>::GetLowerValue(int64_t key) const {
  return (root_ && key != 0) ? root_->GetFloorValue(key - 1, INITIAL_LEVEL) : nullptr;
}

template <typename V>
int LongAdaptiveRadixTreeMap<V>::ForEach(LongObjConsumer<V>* consumer, int limit) const {
  return root_ ? root_->ForEach(consumer, limit) : 0;
}

template <typename V>
int LongAdaptiveRadixTreeMap<V>::ForEachDesc(LongObjConsumer<V>* consumer, int limit) const {
  return root_ ? root_->ForEachDesc(consumer, limit) : 0;
}

template <typename V>
int LongAdaptiveRadixTreeMap<V>::Size(int limit) const {
  return root_ ? std::min(root_->Size(limit), limit) : 0;
}

template <typename V>
std::list<std::pair<int64_t, V*>> LongAdaptiveRadixTreeMap<V>::EntriesList() const {
  return root_ ? root_->Entries() : std::list<std::pair<int64_t, V*>>();
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
std::string LongAdaptiveRadixTreeMap<V>::PrintDiagram(const std::string& prefix,
                                                      int level,
                                                      int nodeLevel,
                                                      int64_t nodeKey,
                                                      int numChildren,
                                                      std::function<uint8_t(int)> getSubKey,
                                                      std::function<void*(int)> getNode) {
  std::string baseKeyPrefix, baseKeyPrefix1;
  const int lvlDiff = level - nodeLevel;
  if (lvlDiff != 0) {
    int chars = lvlDiff >> 2;
    int64_t mask = ((1LL << lvlDiff) - 1LL);
    int64_t keyPart = (nodeKey >> (nodeLevel + 8)) & mask;
    std::ostringstream oss;
    for (int j = 0; j < chars - 2; j++)
      oss << "─";
    oss << "[" << std::hex << std::uppercase << std::setfill('0') << std::setw(chars) << keyPart
        << "]";
    baseKeyPrefix = oss.str();
    baseKeyPrefix1 = std::string(chars * 2, ' ');
  }
  std::ostringstream sb;
  for (int i = 0; i < numChildren; i++) {
    void* node = getNode(i);
    uint8_t subKey = getSubKey(i);
    std::ostringstream keyStream;
    keyStream << baseKeyPrefix << std::hex << std::uppercase << std::setfill('0') << std::setw(2)
              << static_cast<int>(subKey);
    std::string key = keyStream.str();
    std::string x =
      (i == 0) ? (numChildren == 1 ? "──" : "┬─") : (prefix + (i + 1 == numChildren ? "└─" : "├─"));
    if (nodeLevel == 0) {
      sb << x << key << " = " << node;
    } else {
      std::string nextPrefix = prefix + (i + 1 == numChildren ? "    " : "│   ") + baseKeyPrefix1;
      sb << x << key << static_cast<IArtNode<V>*>(node)->PrintDiagram(nextPrefix, nodeLevel - 8);
    }
    if (i < numChildren - 1)
      sb << "\n";
    else if (nodeLevel == 0)
      sb << "\n" << prefix;
  }
  return sb.str();
}

}  // namespace exchange::core::collections::art
