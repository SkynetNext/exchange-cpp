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
#include <cstring>
#include <exchange/core/collections/art/ArtNode16.h>
#include <exchange/core/collections/art/ArtNode4.h>
#include <exchange/core/collections/art/ArtNode48.h>
#include <exchange/core/collections/art/LongAdaptiveRadixTreeMap.h>
#include <exchange/core/collections/art/LongObjConsumer.h>
#include <exchange/core/collections/objpool/ObjectsPool.h>
#include <stdexcept>

namespace exchange {
namespace core {
namespace collections {
namespace art {

template <typename V>
ArtNode16<V>::ArtNode16(
    ::exchange::core::collections::objpool::ObjectsPool *objectsPool)
    : objectsPool_(objectsPool), nodeKey_(0), nodeLevel_(0), numChildren_(0) {
  std::memset(keys_, 0, sizeof(keys_));
  std::memset(nodes_, 0, sizeof(nodes_));
}

template <typename V>
void ArtNode16<V>::InitFromNode4(ArtNode4<V> *node4, int16_t subKey,
                                 void *newElement) {
  const int8_t sourceSize = node4->numChildren_;
  nodeLevel_ = node4->nodeLevel_;
  nodeKey_ = node4->nodeKey_;
  numChildren_ = sourceSize + 1;
  int inserted = 0;
  for (int i = 0; i < sourceSize; i++) {
    const int key = node4->keys_[i];
    if (inserted == 0 && key > subKey) {
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

  // Put original node back into pool
  std::memset(node4->nodes_, 0, sizeof(node4->nodes_));
  objectsPool_->Put(
      ::exchange::core::collections::objpool::ObjectsPool::ART_NODE_4, node4);
}

template <typename V> void ArtNode16<V>::InitFromNode48(ArtNode48<V> *node48) {
  numChildren_ = node48->numChildren_;
  nodeLevel_ = node48->nodeLevel_;
  nodeKey_ = node48->nodeKey_;
  int8_t idx = 0;
  for (int16_t i = 0; i < 256; i++) {
    const int8_t j = node48->indexes_[i];
    if (j != -1) {
      keys_[idx] = i;
      nodes_[idx] = node48->nodes_[j];
      idx++;
    }
    if (idx == numChildren_) {
      break;
    }
  }

  std::memset(node48->nodes_, 0, sizeof(node48->nodes_));
  std::memset(node48->indexes_, -1, sizeof(node48->indexes_));
  objectsPool_->Put(
      ::exchange::core::collections::objpool::ObjectsPool::ART_NODE_48, node48);
}

template <typename V> V *ArtNode16<V>::GetValue(int64_t key, int level) {
  if (level != nodeLevel_ &&
      ((key ^ nodeKey_) & (-1LL << (nodeLevel_ + 8))) != 0) {
    return nullptr;
  }
  const int16_t nodeIndex = static_cast<int16_t>((key >> nodeLevel_) & 0xFF);

  for (int i = 0; i < numChildren_; i++) {
    const int16_t index = keys_[i];
    if (index == nodeIndex) {
      void *node = nodes_[i];
      return nodeLevel_ == 0 ? static_cast<V *>(node)
                             : static_cast<IArtNode<V> *>(node)->GetValue(
                                   key, nodeLevel_ - 8);
    }
    if (nodeIndex < index) {
      break;
    }
  }
  return nullptr;
}

template <typename V>
IArtNode<V> *ArtNode16<V>::Put(int64_t key, int level, V *value) {
  if (level != nodeLevel_) {
    IArtNode<V> *branch = LongAdaptiveRadixTreeMap<V>::BranchIfRequired(
        key, value, nodeKey_, nodeLevel_, this);
    if (branch != nullptr) {
      return branch;
    }
  }

  const int16_t nodeIndex = static_cast<int16_t>((key >> nodeLevel_) & 0xFF);
  int pos = 0;
  while (pos < numChildren_) {
    if (nodeIndex == keys_[pos]) {
      if (nodeLevel_ == 0) {
        nodes_[pos] = value;
      } else {
        IArtNode<V> *oldNode = static_cast<IArtNode<V> *>(nodes_[pos]);
        IArtNode<V> *resizedNode = oldNode->Put(key, nodeLevel_ - 8, value);
        if (resizedNode != nullptr) {
          LongAdaptiveRadixTreeMap<V>::RecycleNodeToPool(oldNode);
          nodes_[pos] = resizedNode;
        }
      }
      return nullptr;
    }
    if (nodeIndex < keys_[pos]) {
      break;
    }
    pos++;
  }

  // New element
  if (numChildren_ != 16) {
    const int copyLength = numChildren_ - pos;
    if (copyLength != 0) {
      std::memmove(keys_ + pos + 1, keys_ + pos, copyLength * sizeof(int16_t));
      std::memmove(nodes_ + pos + 1, nodes_ + pos, copyLength * sizeof(void *));
    }
    keys_[pos] = nodeIndex;
    if (nodeLevel_ == 0) {
      nodes_[pos] = value;
    } else {
      ArtNode4<V> *newSubNode = objectsPool_->Get<ArtNode4<V>>(
          ::exchange::core::collections::objpool::ObjectsPool::ART_NODE_4,
          [this]() { return new ArtNode4<V>(objectsPool_); });
      newSubNode->InitFirstKey(key, value);
      nodes_[pos] = newSubNode;
    }
    numChildren_++;
    return nullptr;
  } else {
    // No space left, create a Node48 with new item
    void *newElement;
    if (nodeLevel_ == 0) {
      newElement = value;
    } else {
      ArtNode4<V> *newSubNode = objectsPool_->Get<ArtNode4<V>>(
          ::exchange::core::collections::objpool::ObjectsPool::ART_NODE_4,
          [this]() { return new ArtNode4<V>(objectsPool_); });
      newSubNode->InitFirstKey(key, value);
      newElement = newSubNode;
    }

    ArtNode48<V> *node48 = objectsPool_->Get<ArtNode48<V>>(
        ::exchange::core::collections::objpool::ObjectsPool::ART_NODE_48,
        [this]() { return new ArtNode48<V>(objectsPool_); });
    node48->InitFromNode16(this, nodeIndex, newElement);

    return node48;
  }
}

template <typename V>
IArtNode<V> *ArtNode16<V>::Remove(int64_t key, int level) {
  if (level != nodeLevel_ &&
      ((key ^ nodeKey_) & (-1LL << (nodeLevel_ + 8))) != 0) {
    return this;
  }
  const int16_t nodeIndex = static_cast<int16_t>((key >> nodeLevel_) & 0xFF);
  void *node = nullptr;
  int pos = 0;
  while (pos < numChildren_) {
    if (nodeIndex == keys_[pos]) {
      node = nodes_[pos];
      break;
    }
    if (nodeIndex < keys_[pos]) {
      return this;
    }
    pos++;
  }

  if (node == nullptr) {
    return this;
  }

  // Removing
  if (nodeLevel_ == 0) {
    RemoveElementAtPos(pos);
  } else {
    IArtNode<V> *oldNode = static_cast<IArtNode<V> *>(node);
    IArtNode<V> *resizedNode = oldNode->Remove(key, nodeLevel_ - 8);
    if (resizedNode != oldNode) {
      LongAdaptiveRadixTreeMap<V>::RecycleNodeToPool(oldNode);
      nodes_[pos] = resizedNode;
      if (resizedNode == nullptr) {
        RemoveElementAtPos(pos);
      }
    }
  }

  // Switch to ArtNode4 if too small
  if (numChildren_ == NODE4_SWITCH_THRESHOLD) {
    ArtNode4<V> *newNode = objectsPool_->Get<ArtNode4<V>>(
        ::exchange::core::collections::objpool::ObjectsPool::ART_NODE_4,
        [this]() { return new ArtNode4<V>(objectsPool_); });
    newNode->InitFromNode16(this);
    return newNode;
  } else {
    return this;
  }
}

template <typename V> V *ArtNode16<V>::GetCeilingValue(int64_t key, int level) {
  if (level != nodeLevel_) {
    const int64_t mask = -1LL << (nodeLevel_ + 8);
    const int64_t keyWithMask = key & mask;
    const int64_t nodeKeyWithMask = nodeKey_ & mask;
    if (nodeKeyWithMask < keyWithMask) {
      return nullptr;
    } else if (keyWithMask != nodeKeyWithMask) {
      key = 0;
    }
  }

  const int16_t nodeIndex = static_cast<int16_t>((key >> nodeLevel_) & 0xFF);

  for (int i = 0; i < numChildren_; i++) {
    const int16_t index = keys_[i];
    if (index == nodeIndex) {
      V *res = nodeLevel_ == 0
                   ? static_cast<V *>(nodes_[i])
                   : static_cast<IArtNode<V> *>(nodes_[i])->GetCeilingValue(
                         key, nodeLevel_ - 8);
      if (res != nullptr) {
        return res;
      }
    }
    if (index > nodeIndex) {
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
    const int64_t keyWithMask = key & mask;
    const int64_t nodeKeyWithMask = nodeKey_ & mask;
    if (nodeKeyWithMask > keyWithMask) {
      return nullptr;
    } else if (keyWithMask != nodeKeyWithMask) {
      key = INT64_MAX;
    }
  }

  const int16_t nodeIndex = static_cast<int16_t>((key >> nodeLevel_) & 0xFF);

  for (int i = numChildren_ - 1; i >= 0; i--) {
    const int16_t index = keys_[i];
    if (index == nodeIndex) {
      V *res = nodeLevel_ == 0
                   ? static_cast<V *>(nodes_[i])
                   : static_cast<IArtNode<V> *>(nodes_[i])->GetFloorValue(
                         key, nodeLevel_ - 8);
      if (res != nullptr) {
        return res;
      }
    }
    if (index < nodeIndex) {
      return nodeLevel_ == 0
                 ? static_cast<V *>(nodes_[i])
                 : static_cast<IArtNode<V> *>(nodes_[i])->GetFloorValue(
                       INT64_MAX, nodeLevel_ - 8);
    }
  }
  return nullptr;
}

template <typename V>
int ArtNode16<V>::ForEach(LongObjConsumer<V> *consumer, int limit) {
  if (nodeLevel_ == 0) {
    const int64_t keyBase = (nodeKey_ >> 8) << 8;
    const int n = std::min(static_cast<int>(numChildren_), limit);
    for (int i = 0; i < n; i++) {
      consumer->Accept(keyBase + keys_[i], static_cast<V *>(nodes_[i]));
    }
    return n;
  } else {
    int numLeft = limit;
    for (int i = 0; i < numChildren_ && numLeft > 0; i++) {
      numLeft -=
          static_cast<IArtNode<V> *>(nodes_[i])->ForEach(consumer, numLeft);
    }
    return limit - numLeft;
  }
}

template <typename V>
int ArtNode16<V>::ForEachDesc(LongObjConsumer<V> *consumer, int limit) {
  if (nodeLevel_ == 0) {
    const int64_t keyBase = (nodeKey_ >> 8) << 8;
    int numFound = 0;
    for (int i = numChildren_ - 1; i >= 0 && numFound < limit; i--) {
      consumer->Accept(keyBase + keys_[i], static_cast<V *>(nodes_[i]));
      numFound++;
    }
    return numFound;
  } else {
    int numLeft = limit;
    for (int i = numChildren_ - 1; i >= 0 && numLeft > 0; i--) {
      numLeft -=
          static_cast<IArtNode<V> *>(nodes_[i])->ForEachDesc(consumer, numLeft);
    }
    return limit - numLeft;
  }
}

template <typename V> int ArtNode16<V>::Size(int limit) {
  if (nodeLevel_ == 0) {
    return numChildren_;
  } else {
    int numLeft = limit;
    for (int i = numChildren_ - 1; i >= 0 && numLeft > 0; i--) {
      numLeft -= static_cast<IArtNode<V> *>(nodes_[i])->Size(numLeft);
    }
    return limit - numLeft;
  }
}

template <typename V> void ArtNode16<V>::RemoveElementAtPos(int pos) {
  const int ppos = pos + 1;
  const int copyLength = numChildren_ - ppos;
  if (copyLength != 0) {
    std::memmove(keys_ + pos, keys_ + ppos, copyLength * sizeof(int16_t));
    std::memmove(nodes_ + pos, nodes_ + ppos, copyLength * sizeof(void *));
  }
  numChildren_--;
  nodes_[numChildren_] = nullptr;
}

template <typename V> void ArtNode16<V>::ValidateInternalState(int level) {
  if (nodeLevel_ > level)
    throw std::runtime_error("unexpected nodeLevel");
  if (numChildren_ > 16 || numChildren_ <= NODE4_SWITCH_THRESHOLD)
    throw std::runtime_error("unexpected numChildren");
  int16_t last = -1;
  for (int i = 0; i < 16; i++) {
    void *node = nodes_[i];
    if (i < numChildren_) {
      if (node == nullptr)
        throw std::runtime_error("null node");
      if (keys_[i] < 0 || keys_[i] >= 256)
        throw std::runtime_error("key out of range");
      if (keys_[i] == last)
        throw std::runtime_error("duplicate key");
      if (keys_[i] < last)
        throw std::runtime_error("wrong key order");
      last = keys_[i];
      if (nodeLevel_ != 0) {
        IArtNode<V> *artNode = static_cast<IArtNode<V> *>(node);
        artNode->ValidateInternalState(nodeLevel_ - 8);
      }
    } else {
      if (node != nullptr)
        throw std::runtime_error("not released node");
    }
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
  const int64_t keyPrefix = (nodeKey_ >> 8) << 8;
  std::list<std::pair<int64_t, V *>> list;
  for (int i = 0; i < numChildren_; i++) {
    if (nodeLevel_ == 0) {
      list.push_back(
          std::make_pair(keyPrefix + keys_[i], static_cast<V *>(nodes_[i])));
    } else {
      auto subEntries = static_cast<IArtNode<V> *>(nodes_[i])->Entries();
      list.splice(list.end(), subEntries);
    }
  }
  return list;
}

template <typename V>
::exchange::core::collections::objpool::ObjectsPool *
ArtNode16<V>::GetObjectsPool() {
  return objectsPool_;
}

// Explicit template instantiations
template class ArtNode16<void>;

// Explicit template instantiations for testing
template class ArtNode16<std::string>;
template class ArtNode16<int64_t>;

} // namespace art
} // namespace collections
} // namespace core
} // namespace exchange
