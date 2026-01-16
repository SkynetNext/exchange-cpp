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

#include "IArtNode.h"
#include "LongObjConsumer.h"
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <exchange/core/collections/objpool/ObjectsPool.h>
#include <list>
#include <stdexcept>
#include <string>

namespace exchange {
namespace core {
namespace collections {
namespace art {

// Forward declarations
template <typename V> class ArtNode4;
template <typename V> class ArtNode48;
template <typename V> class LongAdaptiveRadixTreeMap;
template <typename V>
IArtNode<V> *BranchIfRequired(int64_t key, V *value, int64_t nodeKey,
                              int nodeLevel, IArtNode<V> *caller);
template <typename V> void RecycleNodeToPool(IArtNode<V> *oldNode);

/**
 * ArtNode16 - This node type is used for storing between 5 and 16 child
 * pointers.
 */
template <typename V> class ArtNode16 : public IArtNode<V> {
public:
  static constexpr int NODE4_SWITCH_THRESHOLD = 3;

  explicit ArtNode16(
      ::exchange::core::collections::objpool::ObjectsPool *objectsPool)
      : IArtNode<V>(::exchange::core::collections::objpool::ObjectsPool::ART_NODE_16),
        objectsPool_(objectsPool), nodeKey_(0), nodeLevel_(0), numChildren_(0) {
    std::memset(keys_, 0, sizeof(keys_));
    std::memset(nodes_, 0, sizeof(nodes_));
  }

  V *GetValue(int64_t key, int level) override {
    if (level != nodeLevel_ &&
        ((key ^ nodeKey_) & (-1LL << (nodeLevel_ + 8))) != 0)
      return nullptr;
    const int16_t nodeIndex = static_cast<int16_t>((key >> nodeLevel_) & 0xFF);
    for (int i = 0; i < numChildren_; i++) {
      if (keys_[i] == nodeIndex) {
        void *node = nodes_[i];
        return nodeLevel_ == 0 ? static_cast<V *>(node)
                               : static_cast<IArtNode<V> *>(node)->GetValue(
                                     key, nodeLevel_ - 8);
      }
      if (nodeIndex < keys_[i])
        break;
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

  void InitFromNode4(ArtNode4<V> *node4, int16_t subKey, void *newElement);
  void InitFromNode48(ArtNode48<V> *node48);

  template <typename U> friend class ArtNode4;
  template <typename U> friend class ArtNode48;

private:
  int16_t keys_[16];
  void *nodes_[16];
  ::exchange::core::collections::objpool::ObjectsPool *objectsPool_;
  int64_t nodeKey_;
  int nodeLevel_;
  uint8_t numChildren_;

  void RemoveElementAtPos(int pos) {
    const int copyLength = numChildren_ - (pos + 1);
    if (copyLength > 0) {
      std::memmove(keys_ + pos, keys_ + pos + 1, copyLength * sizeof(int16_t));
      std::memmove(nodes_ + pos, nodes_ + pos + 1, copyLength * sizeof(void *));
    }
    numChildren_--;
    nodes_[numChildren_] = nullptr;
  }
};

// --- Implementation ---

template <typename V>
void ArtNode16<V>::InitFromNode4(ArtNode4<V> *node4, int16_t subKey,
                                 void *newElement) {
  std::memset(keys_, 0, sizeof(keys_));
  std::memset(nodes_, 0, sizeof(nodes_));
  const int sourceSize = node4->numChildren_;
  nodeLevel_ = node4->nodeLevel_;
  nodeKey_ = node4->nodeKey_;
  numChildren_ = sourceSize + 1;
  int inserted = 0;
  for (int i = 0; i < sourceSize; i++) {
    if (inserted == 0 && node4->keys_[i] > subKey) {
      keys_[i] = subKey;
      nodes_[i] = newElement;
      inserted = 1;
    }
    keys_[i + inserted] = node4->keys_[i];
    nodes_[i + inserted] = node4->nodes_[i];
  }
  if (inserted == 0) {
    keys_[sourceSize] = subKey;
    nodes_[sourceSize] = newElement;
  }
}

template <typename V> void ArtNode16<V>::InitFromNode48(ArtNode48<V> *node48) {
  std::memset(keys_, 0, sizeof(keys_));
  std::memset(nodes_, 0, sizeof(nodes_));
  numChildren_ = node48->numChildren_;
  nodeLevel_ = node48->nodeLevel_;
  nodeKey_ = node48->nodeKey_;
  int idx = 0;
  for (int i = 0; i < 256 && idx < numChildren_; i++) {
    if (node48->indexes_[i] != -1) {
      keys_[idx] = i;
      nodes_[idx] = node48->nodes_[node48->indexes_[i]];
      idx++;
    }
  }
}

template <typename V>
IArtNode<V> *ArtNode16<V>::Put(int64_t key, int level, V *value) {
  if (level != nodeLevel_) {
    IArtNode<V> *branch =
        BranchIfRequired<V>(key, value, nodeKey_, nodeLevel_, this);
    if (branch)
      return branch;
  }
  const int16_t nodeIndex = static_cast<int16_t>((key >> nodeLevel_) & 0xFF);
  int pos = 0;
  while (pos < numChildren_) {
    if (keys_[pos] == nodeIndex) {
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
    if (nodeIndex < keys_[pos])
      break;
    pos++;
  }
  if (numChildren_ < 16) {
    const int copyLength = numChildren_ - pos;
    if (copyLength > 0) {
      std::memmove(keys_ + pos + 1, keys_ + pos, copyLength * sizeof(int16_t));
      std::memmove(nodes_ + pos + 1, nodes_ + pos, copyLength * sizeof(void *));
    }
    keys_[pos] = nodeIndex;
    if (nodeLevel_ == 0)
      nodes_[pos] = value;
    else {
      auto *newSub = objectsPool_->template Get<ArtNode4<V>>(
          ::exchange::core::collections::objpool::ObjectsPool::ART_NODE_4,
          [this]() { return new ArtNode4<V>(objectsPool_); });
      newSub->InitFirstKey(key, value);
      nodes_[pos] = newSub;
    }
    numChildren_++;
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
    auto *node48 = objectsPool_->template Get<ArtNode48<V>>(
        ::exchange::core::collections::objpool::ObjectsPool::ART_NODE_48,
        [this]() { return new ArtNode48<V>(objectsPool_); });
    node48->InitFromNode16(this, nodeIndex, newElement);
    RecycleNodeToPool<V>(this);
    return node48;
  }
}

template <typename V>
IArtNode<V> *ArtNode16<V>::Remove(int64_t key, int level) {
  if (level != nodeLevel_ &&
      ((key ^ nodeKey_) & (-1LL << (nodeLevel_ + 8))) != 0)
    return this;
  const int16_t nodeIndex = static_cast<int16_t>((key >> nodeLevel_) & 0xFF);
  int pos = 0;
  while (pos < numChildren_ && keys_[pos] != nodeIndex)
    pos++;
  if (pos == numChildren_)
    return this;
  if (nodeLevel_ == 0) {
    RemoveElementAtPos(pos);
  } else {
    IArtNode<V> *oldSubNode = static_cast<IArtNode<V> *>(nodes_[pos]);
    IArtNode<V> *resizedNode = oldSubNode->Remove(key, nodeLevel_ - 8);
    if (resizedNode != oldSubNode) {
      nodes_[pos] = resizedNode;
      if (resizedNode == nullptr)
        RemoveElementAtPos(pos);
    }
  }
  if (numChildren_ == NODE4_SWITCH_THRESHOLD) {
    auto *node4 = objectsPool_->template Get<ArtNode4<V>>(
        ::exchange::core::collections::objpool::ObjectsPool::ART_NODE_4,
        [this]() { return new ArtNode4<V>(objectsPool_); });
    node4->InitFromNode16(this);
    RecycleNodeToPool<V>(this);
    return node4;
  }
  return this;
}

template <typename V> V *ArtNode16<V>::GetCeilingValue(int64_t key, int level) {
  if (level != nodeLevel_) {
    const int64_t mask = -1LL << (nodeLevel_ + 8);
    if ((nodeKey_ & mask) < (key & mask))
      return nullptr;
    if ((key & mask) != (nodeKey_ & mask))
      key = 0;
  }
  const int16_t nodeIndex = static_cast<int16_t>((key >> nodeLevel_) & 0xFF);
  for (int i = 0; i < numChildren_; i++) {
    if (keys_[i] == nodeIndex) {
      V *res = nodeLevel_ == 0
                   ? static_cast<V *>(nodes_[i])
                   : static_cast<IArtNode<V> *>(nodes_[i])->GetCeilingValue(
                         key, nodeLevel_ - 8);
      if (res)
        return res;
    } else if (keys_[i] > nodeIndex) {
      return nodeLevel_ == 0
                 ? static_cast<V *>(nodes_[i])
                 : static_cast<IArtNode<V> *>(nodes_[i])->GetCeilingValue(
                       0, nodeLevel_ - 8);
    }
  }
  return nullptr;
}

template <typename V> V *ArtNode16<V>::GetFloorValue(int64_t key, int level) {
  if (level != nodeLevel_) {
    const int64_t mask = -1LL << (nodeLevel_ + 8);
    if ((nodeKey_ & mask) > (key & mask))
      return nullptr;
    if ((key & mask) != (nodeKey_ & mask))
      key = INT64_MAX;
  }
  const int16_t nodeIndex = static_cast<int16_t>((key >> nodeLevel_) & 0xFF);
  for (int i = numChildren_ - 1; i >= 0; i--) {
    if (keys_[i] == nodeIndex) {
      V *res = nodeLevel_ == 0
                   ? static_cast<V *>(nodes_[i])
                   : static_cast<IArtNode<V> *>(nodes_[i])->GetFloorValue(
                         key, nodeLevel_ - 8);
      if (res)
        return res;
    } else if (keys_[i] < nodeIndex) {
      return nodeLevel_ == 0
                 ? static_cast<V *>(nodes_[i])
                 : static_cast<IArtNode<V> *>(nodes_[i])->GetFloorValue(
                       INT64_MAX, nodeLevel_ - 8);
    }
  }
  return nullptr;
}

template <typename V>
int ArtNode16<V>::ForEach(LongObjConsumer<V> *consumer, int limit) const {
  if (nodeLevel_ == 0) {
    const int64_t keyBase = nodeKey_ & (-1LL << 8);
    const int n = std::min(static_cast<int>(numChildren_), limit);
    for (int i = 0; i < n; i++)
      consumer->Accept(keyBase + keys_[i], static_cast<V *>(nodes_[i]));
    return n;
  }
  int numLeft = limit;
  for (int i = 0; i < numChildren_ && numLeft > 0; i++)
    numLeft -=
        static_cast<IArtNode<V> *>(nodes_[i])->ForEach(consumer, numLeft);
  return limit - numLeft;
}

template <typename V>
int ArtNode16<V>::ForEachDesc(LongObjConsumer<V> *consumer, int limit) const {
  if (nodeLevel_ == 0) {
    const int64_t keyBase = nodeKey_ & (-1LL << 8);
    int numFound = 0;
    for (int i = numChildren_ - 1; i >= 0 && numFound < limit; i--) {
      consumer->Accept(keyBase + keys_[i], static_cast<V *>(nodes_[i]));
      numFound++;
    }
    return numFound;
  }
  int numLeft = limit;
  for (int i = numChildren_ - 1; i >= 0 && numLeft > 0; i--)
    numLeft -=
        static_cast<IArtNode<V> *>(nodes_[i])->ForEachDesc(consumer, numLeft);
  return limit - numLeft;
}

template <typename V> int ArtNode16<V>::Size(int limit) {
  if (nodeLevel_ == 0)
    return numChildren_;
  int numLeft = limit;
  for (int i = 0; i < numChildren_ && numLeft > 0; i++)
    numLeft -= static_cast<IArtNode<V> *>(nodes_[i])->Size(numLeft);
  return limit - numLeft;
}

template <typename V> void ArtNode16<V>::ValidateInternalState(int level) {
  if (nodeLevel_ > level)
    throw std::runtime_error("unexpected nodeLevel");
  if (numChildren_ > 16 || numChildren_ <= NODE4_SWITCH_THRESHOLD)
    throw std::runtime_error("unexpected numChildren");
  int16_t last = -1;
  for (int i = 0; i < 16; i++) {
    if (i < numChildren_) {
      if (!nodes_[i])
        throw std::runtime_error("null node");
      if (keys_[i] < 0 || keys_[i] >= 256)
        throw std::runtime_error("key out of range");
      if (i > 0 && keys_[i] <= last)
        throw std::runtime_error("wrong key order/duplicate");
      last = keys_[i];
      if (nodeLevel_ != 0)
        static_cast<IArtNode<V> *>(nodes_[i])->ValidateInternalState(
            nodeLevel_ - 8);
    } else if (nodes_[i])
      throw std::runtime_error("not released node");
  }
}

template <typename V>
std::string ArtNode16<V>::PrintDiagram(const std::string &prefix, int level) {
  return LongAdaptiveRadixTreeMap<V>::PrintDiagram(
      prefix, level, nodeLevel_, nodeKey_, numChildren_,
      [this](int idx) { return keys_[idx]; },
      [this](int idx) { return nodes_[idx]; });
}

template <typename V>
std::list<std::pair<int64_t, V *>> ArtNode16<V>::Entries() {
  const int64_t keyPrefix = nodeKey_ & (-1LL << 8);
  std::list<std::pair<int64_t, V *>> list;
  for (int i = 0; i < numChildren_; i++) {
    if (nodeLevel_ == 0)
      list.push_back({keyPrefix + keys_[i], static_cast<V *>(nodes_[i])});
    else {
      auto sub = static_cast<IArtNode<V> *>(nodes_[i])->Entries();
      list.splice(list.end(), sub);
    }
  }
  return list;
}

} // namespace art
} // namespace collections
} // namespace core
} // namespace exchange
