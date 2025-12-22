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
#include <exchange/core/collections/art/LongAdaptiveRadixTreeMap.h>
#include <exchange/core/collections/art/LongObjConsumer.h>
#include <exchange/core/collections/objpool/ObjectsPool.h>
#include <stdexcept>

namespace exchange {
namespace core {
namespace collections {
namespace art {

template <typename V>
ArtNode4<V>::ArtNode4(
    ::exchange::core::collections::objpool::ObjectsPool *objectsPool)
    : objectsPool_(objectsPool), nodeKey_(0), nodeLevel_(0), numChildren_(0) {
  std::memset(keys_, 0, sizeof(keys_));
  std::memset(nodes_, 0, sizeof(nodes_));
}

template <typename V> void ArtNode4<V>::InitFirstKey(int64_t key, V *value) {
  // Clear this node first (it may be from object pool with old data)
  std::memset(keys_, 0, sizeof(keys_));
  std::memset(nodes_, 0, sizeof(nodes_));

  numChildren_ = 1;
  keys_[0] = static_cast<int16_t>(key & 0xFF);
  nodes_[0] = value;
  nodeKey_ = key;
  nodeLevel_ = 0;

  // Clear unused nodes
  nodes_[1] = nullptr;
  nodes_[2] = nullptr;
  nodes_[3] = nullptr;
}

template <typename V>
void ArtNode4<V>::InitTwoKeys(int64_t key1, void *value1, int64_t key2,
                              void *value2, int level) {
  // Clear this node first (it may be from object pool with old data)
  std::memset(keys_, 0, sizeof(keys_));
  std::memset(nodes_, 0, sizeof(nodes_));

  numChildren_ = 2;
  const int16_t idx1 = static_cast<int16_t>((key1 >> level) & 0xFF);
  const int16_t idx2 = static_cast<int16_t>((key2 >> level) & 0xFF);
  // Smallest key first
  if (key1 < key2) {
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
  nodeKey_ = key1; // Leading part the same for both keys
  nodeLevel_ = level;

  // Clear unused nodes
  nodes_[2] = nullptr;
  nodes_[3] = nullptr;
}

template <typename V> void ArtNode4<V>::InitFromNode16(ArtNode16<V> *node16) {
  // Clear this node first (it may be from object pool with old data)
  std::memset(keys_, 0, sizeof(keys_));
  std::memset(nodes_, 0, sizeof(nodes_));

  // Put original node back into pool
  objectsPool_->Put(
      ::exchange::core::collections::objpool::ObjectsPool::ART_NODE_16, node16);

  numChildren_ = node16->numChildren_;
  std::memcpy(keys_, node16->keys_, numChildren_ * sizeof(int16_t));
  std::memcpy(nodes_, node16->nodes_, numChildren_ * sizeof(void *));
  nodeLevel_ = node16->nodeLevel_;
  nodeKey_ = node16->nodeKey_;

  // Clear unused nodes in this node
  for (int i = numChildren_; i < 4; i++) {
    nodes_[i] = nullptr;
  }

  std::memset(node16->nodes_, 0, sizeof(node16->nodes_));
}

template <typename V> V *ArtNode4<V>::GetValue(int64_t key, int level) {
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
      // Can give up searching because keys are in sorted order
      break;
    }
  }
  return nullptr;
}

template <typename V>
IArtNode<V> *ArtNode4<V>::Put(int64_t key, int level, V *value) {
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
      // Just update
      if (nodeLevel_ == 0) {
        nodes_[pos] = value;
      } else {
        IArtNode<V> *oldNode = static_cast<IArtNode<V> *>(nodes_[pos]);
        IArtNode<V> *resizedNode = oldNode->Put(key, nodeLevel_ - 8, value);
        if (resizedNode != nullptr) {
          LongAdaptiveRadixTreeMap<V>::RecycleNodeToPool(oldNode);
          // Update resized node if capacity has increased
          nodes_[pos] = resizedNode;
        }
      }
      return nullptr;
    }
    if (nodeIndex < keys_[pos]) {
      // Can give up searching because keys are in sorted order
      break;
    }
    pos++;
  }

  // New element
  if (numChildren_ != 4) {
    // Capacity less than 4 - can simply insert node
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
    // No space left, create a Node16 with new item
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

    ArtNode16<V> *node16 = objectsPool_->Get<ArtNode16<V>>(
        ::exchange::core::collections::objpool::ObjectsPool::ART_NODE_16,
        [this]() { return new ArtNode16<V>(objectsPool_); });
    node16->InitFromNode4(this, nodeIndex, newElement);

    return node16;
  }
}

template <typename V> IArtNode<V> *ArtNode4<V>::Remove(int64_t key, int level) {
  if (level != nodeLevel_ &&
      ((key ^ nodeKey_) & (-1LL << (nodeLevel_ + 8))) != 0) {
    return this;
  }
  const int16_t nodeIndex = static_cast<int16_t>((key >> nodeLevel_) & 0xFF);
  void *node = nullptr;
  int pos = 0;
  while (pos < numChildren_) {
    if (nodeIndex == keys_[pos]) {
      // Found
      node = nodes_[pos];
      break;
    }
    pos++;
  }

  if (node == nullptr) {
    // Not found
    return this;
  }

  // Removing
  if (nodeLevel_ == 0) {
    RemoveElementAtPos(pos);
  } else {
    IArtNode<V> *oldNode = static_cast<IArtNode<V> *>(node);
    IArtNode<V> *resizedNode = oldNode->Remove(key, nodeLevel_ - 8);
    if (resizedNode != oldNode) {
      // Recycle old node to pool if it was resized
      LongAdaptiveRadixTreeMap<V>::RecycleNodeToPool(oldNode);
      // Update resized node if capacity has decreased
      nodes_[pos] = resizedNode;
      if (resizedNode == nullptr) {
        RemoveElementAtPos(pos);
        if (numChildren_ == 1) {
          // Can merge
          IArtNode<V> *lastNode = static_cast<IArtNode<V> *>(nodes_[0]);
          return lastNode;
        }
      }
    }
  }

  if (numChildren_ == 0) {
    // Indicate if removed last one
    std::memset(nodes_, 0, sizeof(nodes_));
    objectsPool_->Put(
        ::exchange::core::collections::objpool::ObjectsPool::ART_NODE_4, this);
    return nullptr;
  } else {
    return this;
  }
}

template <typename V> V *ArtNode4<V>::GetCeilingValue(int64_t key, int level) {
  // Special processing for compacted nodes
  if (level != nodeLevel_) {
    const int64_t mask = -1LL << (nodeLevel_ + 8);
    const int64_t keyWithMask = key & mask;
    const int64_t nodeKeyWithMask = nodeKey_ & mask;
    if (nodeKeyWithMask < keyWithMask) {
      // Compacted part is lower - no need to search for ceiling entry here
      return nullptr;
    } else if (keyWithMask != nodeKeyWithMask) {
      // Accept first existing entry, because compacted nodekey is higher
      key = 0;
    }
  }

  const int16_t nodeIndex = static_cast<int16_t>((key >> nodeLevel_) & 0xFF);

  for (int i = 0; i < numChildren_; i++) {
    const int16_t index = keys_[i];
    // Any equal or higher is ok
    if (index == nodeIndex) {
      V *res = nodeLevel_ == 0
                   ? static_cast<V *>(nodes_[i])
                   : static_cast<IArtNode<V> *>(nodes_[i])->GetCeilingValue(
                         key, nodeLevel_ - 8);
      if (res != nullptr) {
        // Return if found ceiling, otherwise will try next one
        return res;
      }
    }
    if (index > nodeIndex) {
      // Exploring first higher key
      return nodeLevel_ == 0
                 ? static_cast<V *>(nodes_[i])
                 : static_cast<IArtNode<V> *>(nodes_[i])->GetCeilingValue(
                       0, nodeLevel_ - 8); // Take lowest existing key
    }
  }
  return nullptr;
}

template <typename V> V *ArtNode4<V>::GetFloorValue(int64_t key, int level) {
  // Special processing for compacted nodes
  if (level != nodeLevel_) {
    const int64_t mask = -1LL << (nodeLevel_ + 8);
    const int64_t keyWithMask = key & mask;
    const int64_t nodeKeyWithMask = nodeKey_ & mask;
    if (nodeKeyWithMask > keyWithMask) {
      // Compacted part is higher - no need to search for floor entry here
      return nullptr;
    } else if (keyWithMask != nodeKeyWithMask) {
      // Find highest value, because compacted nodekey is lower
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
        // Return if found floor, otherwise will try prev one
        return res;
      }
    }
    if (index < nodeIndex) {
      // Exploring first lower key
      return nodeLevel_ == 0
                 ? static_cast<V *>(nodes_[i])
                 : static_cast<IArtNode<V> *>(nodes_[i])->GetFloorValue(
                       INT64_MAX,
                       nodeLevel_ - 8); // Take highest existing key
    }
  }
  return nullptr;
}

template <typename V>
int ArtNode4<V>::ForEach(LongObjConsumer<V> *consumer, int limit) {
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
int ArtNode4<V>::ForEachDesc(LongObjConsumer<V> *consumer, int limit) {
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

template <typename V> int ArtNode4<V>::Size(int limit) {
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

template <typename V> void ArtNode4<V>::RemoveElementAtPos(int pos) {
  const int ppos = pos + 1;
  const int copyLength = numChildren_ - ppos;
  if (copyLength != 0) {
    std::memmove(keys_ + pos, keys_ + ppos, copyLength * sizeof(int16_t));
    std::memmove(nodes_ + pos, nodes_ + ppos, copyLength * sizeof(void *));
  }
  numChildren_--;
  nodes_[numChildren_] = nullptr;
}

template <typename V> void ArtNode4<V>::ValidateInternalState(int level) {
  if (nodeLevel_ > level)
    throw std::runtime_error("unexpected nodeLevel");
  if (numChildren_ > 4 || numChildren_ < 1)
    throw std::runtime_error("unexpected numChildren");
  int16_t last = -1;
  for (int i = 0; i < 4; i++) {
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
std::string ArtNode4<V>::PrintDiagram(const std::string &prefix, int level) {
  return LongAdaptiveRadixTreeMap<V>::PrintDiagram(
      prefix, level, nodeLevel_, nodeKey_, numChildren_,
      [this](int idx) { return keys_[idx]; },
      [this](int idx) { return nodes_[idx]; });
}

template <typename V>
std::list<std::pair<int64_t, V *>> ArtNode4<V>::Entries() {
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
ArtNode4<V>::GetObjectsPool() {
  return objectsPool_;
}

// Explicit template instantiations
template class ArtNode4<void>; // For Bucket* and DirectOrder*

// Explicit template instantiations for testing
template class ArtNode4<std::string>;
template class ArtNode4<int64_t>;

} // namespace art
} // namespace collections
} // namespace core
} // namespace exchange
