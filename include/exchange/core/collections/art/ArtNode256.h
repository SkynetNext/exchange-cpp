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

#include <exchange/core/collections/objpool/ObjectsPool.h>
#include <array>
#include <cstdint>
#include <list>
#include <stdexcept>
#include <string>
#include <vector>
#include "IArtNode.h"
#include "LongObjConsumer.h"

namespace exchange::core::collections::art {

// Forward declarations
template <typename V>
class ArtNode4;
template <typename V>
class ArtNode48;
template <typename V>
class LongAdaptiveRadixTreeMap;
template <typename V>
IArtNode<V>*
BranchIfRequired(int64_t key, V* value, int64_t nodeKey, int nodeLevel, IArtNode<V>* caller);
template <typename V>
void RecycleNodeToPool(IArtNode<V>* oldNode);

/**
 * ArtNode256 - The largest node type is simply an array of 256 pointers.
 */
template <typename V>
class ArtNode256 : public IArtNode<V> {
public:
  static constexpr int NODE48_SWITCH_THRESHOLD = 37;

  explicit ArtNode256(::exchange::core::collections::objpool::ObjectsPool* objectsPool)
    : IArtNode<V>(::exchange::core::collections::objpool::ObjectsPool::ART_NODE_256)
    , objectsPool_(objectsPool) {
    nodes_.fill(nullptr);
  }

  V* GetValue(int64_t key, int level) override {
    if (level != nodeLevel_ && ((key ^ nodeKey_) & (-1LL << (nodeLevel_ + 8))) != 0)
      return nullptr;
    const uint8_t idx = static_cast<uint8_t>((key >> nodeLevel_) & 0xFF);
    void* node = nodes_[idx];
    if (node)
      return nodeLevel_ == 0 ? static_cast<V*>(node)
                             : static_cast<IArtNode<V>*>(node)->GetValue(key, nodeLevel_ - 8);
    return nullptr;
  }

  IArtNode<V>* Put(int64_t key, int level, V* value) override;
  IArtNode<V>* Remove(int64_t key, int level) override;
  V* GetCeilingValue(int64_t key, int level) override;
  V* GetFloorValue(int64_t key, int level) override;
  int ForEach(LongObjConsumer<V>* consumer, int limit) const override;
  int ForEachDesc(LongObjConsumer<V>* consumer, int limit) const override;
  int Size(int limit) override;
  void ValidateInternalState(int level) override;
  std::string PrintDiagram(const std::string& prefix, int level) override;
  std::list<std::pair<int64_t, V*>> Entries() override;

  ::exchange::core::collections::objpool::ObjectsPool* GetObjectsPool() override {
    return objectsPool_;
  }

  void RecycleTree() override {
    // Recursively recycle child nodes first
    if (nodeLevel_ != 0) {
      for (int i = 0; i < 256; i++) {
        if (nodes_[i] != nullptr) {
          static_cast<IArtNode<V>*>(nodes_[i])->RecycleTree();
        }
      }
    }
    // Then recycle this node
    RecycleNodeToPool<V>(this);
  }

  void InitFromNode48(ArtNode48<V>* node48, uint8_t subKey, void* newElement);

  template <typename U>
  friend class ArtNode48;

private:
  std::array<void*, 256> nodes_{};
  ::exchange::core::collections::objpool::ObjectsPool* objectsPool_;
  int64_t nodeKey_ = 0;
  int nodeLevel_ = 0;
  uint16_t numChildren_ = 0;

  std::vector<uint8_t> CreateKeysArray() {
    std::vector<uint8_t> keys;
    keys.reserve(numChildren_);
    for (int i = 0; i < 256; i++)
      if (nodes_[i])
        keys.push_back(i);
    return keys;
  }
};

// --- Implementation ---

template <typename V>
void ArtNode256<V>::InitFromNode48(ArtNode48<V>* node48, uint8_t subKey, void* newElement) {
  nodes_.fill(nullptr);
  nodeLevel_ = node48->nodeLevel_;
  nodeKey_ = node48->nodeKey_;
  numChildren_ = node48->numChildren_ + 1;
  for (int i = 0; i < 256; i++)
    if (node48->indexes_[i] != -1)
      nodes_[i] = node48->nodes_[node48->indexes_[i]];
  nodes_[subKey] = newElement;
}

template <typename V>
IArtNode<V>* ArtNode256<V>::Put(int64_t key, int level, V* value) {
  if (level != nodeLevel_) {
    IArtNode<V>* branch = BranchIfRequired<V>(key, value, nodeKey_, nodeLevel_, this);
    if (branch)
      return branch;
  }
  const uint8_t idx = static_cast<uint8_t>((key >> nodeLevel_) & 0xFF);
  if (!nodes_[idx]) {
    numChildren_++;
    if (nodeLevel_ == 0)
      nodes_[idx] = value;
    else {
      auto* newSub = objectsPool_->template Get<ArtNode4<V>>(
        ::exchange::core::collections::objpool::ObjectsPool::ART_NODE_4,
        [this]() { return new ArtNode4<V>(objectsPool_); });
      newSub->InitFirstKey(key, value);
      nodes_[idx] = newSub;
    }
  } else {
    if (nodeLevel_ == 0)
      nodes_[idx] = value;
    else {
      IArtNode<V>* oldSubNode = static_cast<IArtNode<V>*>(nodes_[idx]);
      IArtNode<V>* resizedNode = oldSubNode->Put(key, nodeLevel_ - 8, value);
      if (resizedNode != nullptr) {
        nodes_[idx] = resizedNode;
      }
    }
  }
  return nullptr;
}

template <typename V>
IArtNode<V>* ArtNode256<V>::Remove(int64_t key, int level) {
  if (level != nodeLevel_ && ((key ^ nodeKey_) & (-1LL << (nodeLevel_ + 8))) != 0)
    return this;
  const uint8_t idx = static_cast<uint8_t>((key >> nodeLevel_) & 0xFF);
  if (!nodes_[idx])
    return this;
  if (nodeLevel_ == 0) {
    nodes_[idx] = nullptr;
    numChildren_--;
  } else {
    IArtNode<V>* oldSubNode = static_cast<IArtNode<V>*>(nodes_[idx]);
    IArtNode<V>* resizedNode = oldSubNode->Remove(key, nodeLevel_ - 8);
    if (resizedNode != oldSubNode) {
      nodes_[idx] = resizedNode;
      if (resizedNode == nullptr)
        numChildren_--;
    }
  }
  if (numChildren_ == NODE48_SWITCH_THRESHOLD) {
    auto* node48 = objectsPool_->template Get<ArtNode48<V>>(
      ::exchange::core::collections::objpool::ObjectsPool::ART_NODE_48,
      [this]() { return new ArtNode48<V>(objectsPool_); });
    node48->InitFromNode256(this);
    RecycleNodeToPool<V>(this);
    return node48;
  }
  return this;
}

template <typename V>
V* ArtNode256<V>::GetCeilingValue(int64_t key, int level) {
  if (level != nodeLevel_) {
    const int64_t mask = -1LL << (nodeLevel_ + 8);
    if ((nodeKey_ & mask) < (key & mask))
      return nullptr;
    if ((key & mask) != (nodeKey_ & mask))
      key = 0;
  }
  int idx = static_cast<int>((key >> nodeLevel_) & 0xFF);
  for (; idx < 256; idx++) {
    if (nodes_[idx]) {
      V* res = nodeLevel_ == 0
                 ? static_cast<V*>(nodes_[idx])
                 : static_cast<IArtNode<V>*>(nodes_[idx])->GetCeilingValue(key, nodeLevel_ - 8);
      if (res)
        return res;
      key = 0;
    }
  }
  return nullptr;
}

template <typename V>
V* ArtNode256<V>::GetFloorValue(int64_t key, int level) {
  if (level != nodeLevel_) {
    const int64_t mask = -1LL << (nodeLevel_ + 8);
    if ((nodeKey_ & mask) > (key & mask))
      return nullptr;
    if ((key & mask) != (nodeKey_ & mask))
      key = INT64_MAX;
  }
  int idx = static_cast<int>((key >> nodeLevel_) & 0xFF);
  for (; idx >= 0; idx--) {
    if (nodes_[idx]) {
      V* res = nodeLevel_ == 0
                 ? static_cast<V*>(nodes_[idx])
                 : static_cast<IArtNode<V>*>(nodes_[idx])->GetFloorValue(key, nodeLevel_ - 8);
      if (res)
        return res;
      key = INT64_MAX;
    }
  }
  return nullptr;
}

template <typename V>
int ArtNode256<V>::ForEach(LongObjConsumer<V>* consumer, int limit) const {
  int numLeft = limit;
  for (int i = 0; i < 256 && numLeft > 0; i++) {
    if (nodes_[i]) {
      if (nodeLevel_ == 0) {
        consumer->Accept((nodeKey_ & (-1LL << 8)) + i, static_cast<V*>(nodes_[i]));
        numLeft--;
      } else
        numLeft -= static_cast<IArtNode<V>*>(nodes_[i])->ForEach(consumer, numLeft);
    }
  }
  return limit - numLeft;
}

template <typename V>
int ArtNode256<V>::ForEachDesc(LongObjConsumer<V>* consumer, int limit) const {
  int numLeft = limit;
  for (int i = 255; i >= 0 && numLeft > 0; i--) {
    if (nodes_[i]) {
      if (nodeLevel_ == 0) {
        consumer->Accept((nodeKey_ & (-1LL << 8)) + i, static_cast<V*>(nodes_[i]));
        numLeft--;
      } else
        numLeft -= static_cast<IArtNode<V>*>(nodes_[i])->ForEachDesc(consumer, numLeft);
    }
  }
  return limit - numLeft;
}

template <typename V>
int ArtNode256<V>::Size(int limit) {
  if (nodeLevel_ == 0)
    return numChildren_;
  int numLeft = limit;
  for (int i = 0; i < 256 && numLeft > 0; i++)
    if (nodes_[i])
      numLeft -= static_cast<IArtNode<V>*>(nodes_[i])->Size(numLeft);
  return limit - numLeft;
}

template <typename V>
void ArtNode256<V>::ValidateInternalState(int level) {
  if (nodeLevel_ > level)
    throw std::runtime_error("unexpected nodeLevel");
  int found = 0;
  for (int i = 0; i < 256; i++) {
    if (nodes_[i]) {
      found++;
      if (nodeLevel_ != 0)
        static_cast<IArtNode<V>*>(nodes_[i])->ValidateInternalState(nodeLevel_ - 8);
    }
  }
  if (found != numChildren_ || numChildren_ <= NODE48_SWITCH_THRESHOLD)
    throw std::runtime_error("wrong numChildren");
}

template <typename V>
std::string ArtNode256<V>::PrintDiagram(const std::string& prefix, int level) {
  auto keys = CreateKeysArray();
  return LongAdaptiveRadixTreeMap<V>::PrintDiagram(
    prefix, level, nodeLevel_, nodeKey_, numChildren_, [&keys](int idx) { return keys[idx]; },
    [this, &keys](int idx) { return nodes_[keys[idx]]; });
}

template <typename V>
std::list<std::pair<int64_t, V*>> ArtNode256<V>::Entries() {
  const int64_t keyPrefix = nodeKey_ & (-1LL << 8);
  std::list<std::pair<int64_t, V*>> list;
  for (int i = 0; i < 256; i++) {
    if (nodes_[i]) {
      if (nodeLevel_ == 0)
        list.push_back({keyPrefix + i, static_cast<V*>(nodes_[i])});
      else {
        auto sub = static_cast<IArtNode<V>*>(nodes_[i])->Entries();
        list.splice(list.end(), sub);
      }
    }
  }
  return list;
}

}  // namespace exchange::core::collections::art
