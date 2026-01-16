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
template <typename V> class ArtNode16;
template <typename V> class LongAdaptiveRadixTreeMap;
template <typename V>
IArtNode<V> *BranchIfRequired(int64_t key, V *value, int64_t nodeKey,
                              int nodeLevel, IArtNode<V> *caller);
template <typename V> void RecycleNodeToPool(IArtNode<V> *oldNode);

/**
 * ArtNode4 - The smallest node type can store up to 4 child pointers
 */
template <typename V> class ArtNode4 : public IArtNode<V> {
public:
  explicit ArtNode4(
      ::exchange::core::collections::objpool::ObjectsPool *objectsPool)
      : IArtNode<V>(::exchange::core::collections::objpool::ObjectsPool::ART_NODE_4),
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

  void InitFirstKey(int64_t key, V *value) {
    std::memset(keys_, 0, sizeof(keys_));
    std::memset(nodes_, 0, sizeof(nodes_));
    numChildren_ = 1;
    keys_[0] = static_cast<int16_t>(key & 0xFF);
    nodes_[0] = value;
    nodeKey_ = key;
    nodeLevel_ = 0;
  }

  void InitTwoKeys(int64_t key1, void *value1, int64_t key2, void *value2,
                   int level) {
    std::memset(keys_, 0, sizeof(keys_));
    std::memset(nodes_, 0, sizeof(nodes_));
    numChildren_ = 2;
    const int16_t idx1 = static_cast<int16_t>((key1 >> level) & 0xFF);
    const int16_t idx2 = static_cast<int16_t>((key2 >> level) & 0xFF);
    if (idx1 < idx2) {
      keys_[0] = idx1;
      nodes_[0] = value1;
      keys_[1] = idx2;
      nodes_[1] = value2;
    } else {
      keys_[0] = idx2;
      nodes_[0] = value2;
      keys_[1] = idx1;
      nodes_[1] = value1;
    }
    nodeKey_ = key1 & (-1LL << level);
    nodeLevel_ = level;
  }

  void InitFromNode16(ArtNode16<V> *node16) {
    std::memset(keys_, 0, sizeof(keys_));
    std::memset(nodes_, 0, sizeof(nodes_));
    numChildren_ = node16->numChildren_;
    std::memcpy(keys_, node16->keys_, numChildren_ * sizeof(int16_t));
    std::memcpy(nodes_, node16->nodes_, numChildren_ * sizeof(void *));
    nodeLevel_ = node16->nodeLevel_;
    nodeKey_ = node16->nodeKey_;
    for (int i = numChildren_; i < 4; i++)
      nodes_[i] = nullptr;
    // node16 should be recycled by the caller
  }

  template <typename U> friend class ArtNode16;

private:
  int16_t keys_[4];
  void *nodes_[4];
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
IArtNode<V> *ArtNode4<V>::Put(int64_t key, int level, V *value) {
  if (level != nodeLevel_) {
    IArtNode<V> *branch =
        BranchIfRequired<V>(key, value, nodeKey_, nodeLevel_, this);
    if (branch)
      return branch;
  }
  const int16_t nodeIndex = static_cast<int16_t>((key >> nodeLevel_) & 0xFF);
  int pos = 0;
  while (pos < numChildren_) {
    if (nodeIndex == keys_[pos]) {
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
  if (numChildren_ < 4) {
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
    auto *node16 = objectsPool_->template Get<ArtNode16<V>>(
        ::exchange::core::collections::objpool::ObjectsPool::ART_NODE_16,
        [this]() { return new ArtNode16<V>(objectsPool_); });
    node16->InitFromNode4(this, nodeIndex, newElement);
    RecycleNodeToPool<V>(this);
    return node16;
  }
}

template <typename V> IArtNode<V> *ArtNode4<V>::Remove(int64_t key, int level) {
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
    IArtNode<V> *oldNode = static_cast<IArtNode<V> *>(nodes_[pos]);
    IArtNode<V> *resizedNode = oldNode->Remove(key, nodeLevel_ - 8);
    if (resizedNode != oldNode) {
      nodes_[pos] = resizedNode;
      if (resizedNode == nullptr) {
        RemoveElementAtPos(pos);
        if (numChildren_ == 1 && nodeLevel_ != 0) {
          IArtNode<V> *lastNode = static_cast<IArtNode<V> *>(nodes_[0]);
          nodes_[0] = nullptr;
          RecycleNodeToPool<V>(this);
          return lastNode;
        }
      }
    }
  }
  if (numChildren_ == 0) {
    RecycleNodeToPool<V>(this);
    return nullptr;
  }
  return this;
}

template <typename V> V *ArtNode4<V>::GetCeilingValue(int64_t key, int level) {
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

template <typename V> V *ArtNode4<V>::GetFloorValue(int64_t key, int level) {
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
int ArtNode4<V>::ForEach(LongObjConsumer<V> *consumer, int limit) const {
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
int ArtNode4<V>::ForEachDesc(LongObjConsumer<V> *consumer, int limit) const {
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

template <typename V> int ArtNode4<V>::Size(int limit) {
  if (nodeLevel_ == 0)
    return numChildren_;
  int numLeft = limit;
  for (int i = 0; i < numChildren_ && numLeft > 0; i++)
    numLeft -= static_cast<IArtNode<V> *>(nodes_[i])->Size(numLeft);
  return limit - numLeft;
}

template <typename V> void ArtNode4<V>::ValidateInternalState(int level) {
  if (nodeLevel_ > level)
    throw std::runtime_error("unexpected nodeLevel");
  if (numChildren_ > 4 || numChildren_ < 1)
    throw std::runtime_error("unexpected numChildren");
  int16_t last = -1;
  for (int i = 0; i < 4; i++) {
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
std::string ArtNode4<V>::PrintDiagram(const std::string &prefix, int level) {
  return LongAdaptiveRadixTreeMap<V>::PrintDiagram(
      prefix, level, nodeLevel_, nodeKey_, numChildren_,
      [this](int idx) { return keys_[idx]; },
      [this](int idx) { return nodes_[idx]; });
}

template <typename V>
std::list<std::pair<int64_t, V *>> ArtNode4<V>::Entries() {
  const int64_t keyPrefix = nodeKey_ & (-1LL << 8);
  std::list<std::pair<int64_t, V *>> list;
  for (int i = 0; i < numChildren_; i++) {
    if (nodeLevel_ == 0)
      list.push_back(
          {keyPrefix + (keys_[i] & 0xFF), static_cast<V *>(nodes_[i])});
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
