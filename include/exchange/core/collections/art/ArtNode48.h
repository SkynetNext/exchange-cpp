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
#include <cstring>
#include <exchange/core/collections/objpool/ObjectsPool.h>
#include <list>
#include <stdexcept>
#include <string>
#include <vector>

namespace exchange {
namespace core {
namespace collections {
namespace art {

// Forward declarations
template <typename V> class ArtNode4;
template <typename V> class ArtNode16;
template <typename V> class ArtNode256;
template <typename V> class LongAdaptiveRadixTreeMap;
template <typename V>
IArtNode<V> *BranchIfRequired(int64_t key, V *value, int64_t nodeKey,
                              int nodeLevel, IArtNode<V> *caller);
template <typename V> void RecycleNodeToPool(IArtNode<V> *oldNode);

/**
 * ArtNode48 - For storing between 17 and 48 child pointers.
 */
template <typename V> class ArtNode48 : public IArtNode<V> {
public:
  static constexpr int NODE16_SWITCH_THRESHOLD = 12;

  explicit ArtNode48(
      ::exchange::core::collections::objpool::ObjectsPool *objectsPool)
      : objectsPool_(objectsPool), nodeKey_(0), nodeLevel_(0), numChildren_(0),
        freeBitMask_(0) {
    std::memset(indexes_, -1, sizeof(indexes_));
    std::memset(nodes_, 0, sizeof(nodes_));
  }

  V *GetValue(int64_t key, int level) override {
    if (level != nodeLevel_ &&
        ((key ^ nodeKey_) & (-1LL << (nodeLevel_ + 8))) != 0)
      return nullptr;
    const int16_t idx = static_cast<int16_t>((key >> nodeLevel_) & 0xFF);
    const int8_t nodeIndex = indexes_[idx];
    if (nodeIndex != -1) {
      void *node = nodes_[nodeIndex];
      return nodeLevel_ == 0 ? static_cast<V *>(node)
                             : static_cast<IArtNode<V> *>(node)->GetValue(
                                   key, nodeLevel_ - 8);
    }
    return nullptr;
  }

  IArtNode<V> *Put(int64_t key, int level, V *value) override;
  IArtNode<V> *Remove(int64_t key, int level) override;
  V *GetCeilingValue(int64_t key, int level) override;
  V *GetFloorValue(int64_t key, int level) override;
  int ForEach(LongObjConsumer<V> *consumer, int limit) const override;
  int ForEachDesc(LongObjConsumer<V> *consumer, int limit) const override;
  int Size(int limit) override;
  void ValidateInternalState(int level) override;
  std::string PrintDiagram(const std::string &prefix, int level) override;
  std::list<std::pair<int64_t, V *>> Entries() override;
  ::exchange::core::collections::objpool::ObjectsPool *
  GetObjectsPool() override {
    return objectsPool_;
  }

  void InitFromNode16(ArtNode16<V> *node16, int16_t subKey, void *newElement);
  void InitFromNode256(ArtNode256<V> *node256);

  template <typename U> friend class ArtNode16;
  template <typename U> friend class ArtNode256;

private:
  int8_t indexes_[256];
  void *nodes_[48];
  ::exchange::core::collections::objpool::ObjectsPool *objectsPool_;
  int64_t nodeKey_;
  int nodeLevel_;
  uint8_t numChildren_;
  int64_t freeBitMask_;

  std::vector<int16_t> CreateKeysArray() {
    std::vector<int16_t> keys;
    keys.reserve(numChildren_);
    for (int i = 0; i < 256; i++)
      if (indexes_[i] != -1)
        keys.push_back(i);
    return keys;
  }
};

// --- Implementation ---

template <typename V>
void ArtNode48<V>::InitFromNode16(ArtNode16<V> *node16, int16_t subKey,
                                  void *newElement) {
  std::memset(indexes_, -1, sizeof(indexes_));
  std::memset(nodes_, 0, sizeof(nodes_));
  numChildren_ = node16->numChildren_ + 1;
  nodeLevel_ = node16->nodeLevel_;
  nodeKey_ = node16->nodeKey_;
  for (int i = 0; i < node16->numChildren_; i++) {
    indexes_[node16->keys_[i]] = i;
    nodes_[i] = node16->nodes_[i];
  }
  indexes_[subKey] = node16->numChildren_;
  nodes_[node16->numChildren_] = newElement;
  freeBitMask_ = (1LL << numChildren_) - 1;
}

template <typename V>
void ArtNode48<V>::InitFromNode256(ArtNode256<V> *node256) {
  std::memset(indexes_, -1, sizeof(indexes_));
  std::memset(nodes_, 0, sizeof(nodes_));
  numChildren_ = node256->numChildren_;
  nodeLevel_ = node256->nodeLevel_;
  nodeKey_ = node256->nodeKey_;
  int idx = 0;
  for (int i = 0; i < 256 && idx < numChildren_; i++) {
    if (node256->nodes_[i]) {
      indexes_[i] = idx;
      nodes_[idx] = node256->nodes_[i];
      idx++;
    }
  }
  freeBitMask_ = (1LL << numChildren_) - 1;
}

template <typename V>
IArtNode<V> *ArtNode48<V>::Put(int64_t key, int level, V *value) {
  if (level != nodeLevel_) {
    IArtNode<V> *branch =
        BranchIfRequired<V>(key, value, nodeKey_, nodeLevel_, this);
    if (branch)
      return branch;
  }
  const int16_t subKey = static_cast<int16_t>((key >> nodeLevel_) & 0xFF);
  const int8_t pos = indexes_[subKey];
  if (pos != -1) {
    if (nodeLevel_ == 0)
      nodes_[pos] = value;
    else {
      IArtNode<V> *oldSubNode = static_cast<IArtNode<V> *>(nodes_[pos]);
      IArtNode<V> *resizedNode = oldSubNode->Put(key, nodeLevel_ - 8, value);
      if (resizedNode != nullptr) {
        nodes_[pos] = resizedNode;
      }
    }
    return nullptr;
  }
  if (numChildren_ < 48) {
    const int8_t freePos = static_cast<int8_t>(__builtin_ctzll(~freeBitMask_));
    indexes_[subKey] = freePos;
    if (nodeLevel_ == 0)
      nodes_[freePos] = value;
    else {
      auto *newSub = objectsPool_->template Get<ArtNode4<V>>(
          ::exchange::core::collections::objpool::ObjectsPool::ART_NODE_4,
          [this]() { return new ArtNode4<V>(objectsPool_); });
      newSub->InitFirstKey(key, value);
      nodes_[freePos] = newSub;
    }
    numChildren_++;
    freeBitMask_ |= (1LL << freePos);
    return nullptr;
  } else {
    void *newElement;
    if (nodeLevel_ == 0)
      newElement = value;
    else {
      auto *newSub = objectsPool_->template Get<ArtNode4<V>>(
          ::exchange::core::collections::objpool::ObjectsPool::ART_NODE_4,
          [this]() { return new ArtNode4<V>(objectsPool_); });
      newSub->InitFirstKey(key, value);
      newElement = newSub;
    }
    auto *node256 = objectsPool_->template Get<ArtNode256<V>>(
        ::exchange::core::collections::objpool::ObjectsPool::ART_NODE_256,
        [this]() { return new ArtNode256<V>(objectsPool_); });
    node256->InitFromNode48(this, subKey, newElement);
    RecycleNodeToPool<V>(this);
    return node256;
  }
}

template <typename V>
IArtNode<V> *ArtNode48<V>::Remove(int64_t key, int level) {
  if (level != nodeLevel_ &&
      ((key ^ nodeKey_) & (-1LL << (nodeLevel_ + 8))) != 0)
    return this;
  const int16_t subKey = static_cast<int16_t>((key >> nodeLevel_) & 0xFF);
  const int8_t pos = indexes_[subKey];
  if (pos == -1)
    return this;
  if (nodeLevel_ == 0) {
    indexes_[subKey] = -1;
    nodes_[pos] = nullptr;
    numChildren_--;
    freeBitMask_ &= ~(1LL << pos);
  } else {
    IArtNode<V> *oldSubNode = static_cast<IArtNode<V> *>(nodes_[pos]);
    IArtNode<V> *resizedNode = oldSubNode->Remove(key, nodeLevel_ - 8);
    if (resizedNode != oldSubNode) {
      nodes_[pos] = resizedNode;
      if (resizedNode == nullptr) {
        indexes_[subKey] = -1;
        numChildren_--;
        freeBitMask_ &= ~(1LL << pos);
      }
    }
  }
  if (numChildren_ == NODE16_SWITCH_THRESHOLD) {
    auto *node16 = objectsPool_->template Get<ArtNode16<V>>(
        ::exchange::core::collections::objpool::ObjectsPool::ART_NODE_16,
        [this]() { return new ArtNode16<V>(objectsPool_); });
    node16->InitFromNode48(this);
    RecycleNodeToPool<V>(this);
    return node16;
  }
  return this;
}

template <typename V> V *ArtNode48<V>::GetCeilingValue(int64_t key, int level) {
  if (level != nodeLevel_) {
    const int64_t mask = -1LL << (nodeLevel_ + 8);
    if ((nodeKey_ & mask) < (key & mask))
      return nullptr;
    if ((key & mask) != (nodeKey_ & mask))
      key = 0;
  }
  int16_t subKey = static_cast<int16_t>((key >> nodeLevel_) & 0xFF);
  for (; subKey < 256; subKey++) {
    int8_t pos = indexes_[subKey];
    if (pos != -1) {
      V *res = nodeLevel_ == 0 ? static_cast<V *>(nodes_[pos])
                               : static_cast<IArtNode<V> *>(nodes_[pos])
                                     ->GetCeilingValue(key, nodeLevel_ - 8);
      if (res)
        return res;
      key = 0; // After first check, use 0 for next sub-nodes
    }
  }
  return nullptr;
}

template <typename V> V *ArtNode48<V>::GetFloorValue(int64_t key, int level) {
  if (level != nodeLevel_) {
    const int64_t mask = -1LL << (nodeLevel_ + 8);
    if ((nodeKey_ & mask) > (key & mask))
      return nullptr;
    if ((key & mask) != (nodeKey_ & mask))
      key = INT64_MAX;
  }
  int16_t subKey = static_cast<int16_t>((key >> nodeLevel_) & 0xFF);
  for (; subKey >= 0; subKey--) {
    int8_t pos = indexes_[subKey];
    if (pos != -1) {
      V *res = nodeLevel_ == 0 ? static_cast<V *>(nodes_[pos])
                               : static_cast<IArtNode<V> *>(nodes_[pos])
                                     ->GetFloorValue(key, nodeLevel_ - 8);
      if (res)
        return res;
      key = INT64_MAX;
    }
  }
  return nullptr;
}

template <typename V>
int ArtNode48<V>::ForEach(LongObjConsumer<V> *consumer, int limit) const {
  int numLeft = limit;
  for (int i = 0; i < 256 && numLeft > 0; i++) {
    if (indexes_[i] != -1) {
      if (nodeLevel_ == 0) {
        consumer->Accept((nodeKey_ & (-1LL << 8)) + i,
                         static_cast<V *>(nodes_[indexes_[i]]));
        numLeft--;
      } else
        numLeft -= static_cast<IArtNode<V> *>(nodes_[indexes_[i]])
                       ->ForEach(consumer, numLeft);
    }
  }
  return limit - numLeft;
}

template <typename V>
int ArtNode48<V>::ForEachDesc(LongObjConsumer<V> *consumer, int limit) const {
  int numLeft = limit;
  for (int i = 255; i >= 0 && numLeft > 0; i--) {
    if (indexes_[i] != -1) {
      if (nodeLevel_ == 0) {
        consumer->Accept((nodeKey_ & (-1LL << 8)) + i,
                         static_cast<V *>(nodes_[indexes_[i]]));
        numLeft--;
      } else
        numLeft -= static_cast<IArtNode<V> *>(nodes_[indexes_[i]])
                       ->ForEachDesc(consumer, numLeft);
    }
  }
  return limit - numLeft;
}

template <typename V> int ArtNode48<V>::Size(int limit) {
  if (nodeLevel_ == 0)
    return numChildren_;
  int numLeft = limit;
  for (int i = 0; i < 256 && numLeft > 0; i++)
    if (indexes_[i] != -1)
      numLeft -= static_cast<IArtNode<V> *>(nodes_[indexes_[i]])->Size(numLeft);
  return limit - numLeft;
}

template <typename V> void ArtNode48<V>::ValidateInternalState(int level) {
  if (nodeLevel_ > level)
    throw std::runtime_error("unexpected nodeLevel");
  int found = 0;
  int64_t expectedMask = 0;
  for (int i = 0; i < 256; i++) {
    if (indexes_[i] != -1) {
      found++;
      expectedMask |= (1LL << indexes_[i]);
      if (!nodes_[indexes_[i]])
        throw std::runtime_error("null node");
      if (nodeLevel_ != 0)
        static_cast<IArtNode<V> *>(nodes_[indexes_[i]])
            ->ValidateInternalState(nodeLevel_ - 8);
    }
  }
  if (found != numChildren_ || expectedMask != freeBitMask_)
    throw std::runtime_error("wrong numChildren/mask");
}

template <typename V>
std::string ArtNode48<V>::PrintDiagram(const std::string &prefix, int level) {
  auto keys = CreateKeysArray();
  return LongAdaptiveRadixTreeMap<V>::PrintDiagram(
      prefix, level, nodeLevel_, nodeKey_, numChildren_,
      [&keys](int idx) { return keys[idx]; },
      [this, &keys](int idx) { return nodes_[indexes_[keys[idx]]]; });
}

template <typename V>
std::list<std::pair<int64_t, V *>> ArtNode48<V>::Entries() {
  const int64_t keyPrefix = nodeKey_ & (-1LL << 8);
  std::list<std::pair<int64_t, V *>> list;
  for (int i = 0; i < 256; i++) {
    if (indexes_[i] != -1) {
      if (nodeLevel_ == 0)
        list.push_back({keyPrefix + i, static_cast<V *>(nodes_[indexes_[i]])});
      else {
        auto sub = static_cast<IArtNode<V> *>(nodes_[indexes_[i]])->Entries();
        list.splice(list.end(), sub);
      }
    }
  }
  return list;
}

} // namespace art
} // namespace collections
} // namespace core
} // namespace exchange
