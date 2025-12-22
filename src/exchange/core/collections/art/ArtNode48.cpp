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

#include <cstring>
#include <exchange/core/collections/art/ArtNode16.h>
#include <exchange/core/collections/art/ArtNode256.h>
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
ArtNode48<V>::ArtNode48(
    ::exchange::core::collections::objpool::ObjectsPool *objectsPool)
    : objectsPool_(objectsPool), nodeKey_(0), nodeLevel_(0), numChildren_(0),
      freeBitMask_(0) {
  std::memset(indexes_, -1, sizeof(indexes_));
  std::memset(nodes_, 0, sizeof(nodes_));
}

template <typename V>
void ArtNode48<V>::InitFromNode16(ArtNode16<V> *node16, int16_t subKey,
                                  void *newElement) {
  const int8_t sourceSize = 16;
  std::memset(indexes_, -1, sizeof(indexes_));
  numChildren_ = sourceSize + 1;
  nodeLevel_ = node16->nodeLevel_;
  nodeKey_ = node16->nodeKey_;

  for (int8_t i = 0; i < sourceSize; i++) {
    indexes_[node16->keys_[i]] = i;
    nodes_[i] = node16->nodes_[i];
  }

  indexes_[subKey] = sourceSize;
  nodes_[sourceSize] = newElement;
  freeBitMask_ = (1LL << (sourceSize + 1)) - 1;

  std::memset(node16->nodes_, 0, sizeof(node16->nodes_));
  objectsPool_->Put(
      ::exchange::core::collections::objpool::ObjectsPool::ART_NODE_16, node16);
}

template <typename V>
void ArtNode48<V>::InitFromNode256(ArtNode256<V> *node256) {
  std::memset(indexes_, -1, sizeof(indexes_));
  numChildren_ = static_cast<int8_t>(node256->numChildren_);
  nodeLevel_ = node256->nodeLevel_;
  nodeKey_ = node256->nodeKey_;
  int8_t idx = 0;
  for (int i = 0; i < 256; i++) {
    void *node = node256->nodes_[i];
    if (node != nullptr) {
      indexes_[i] = idx;
      nodes_[idx] = node;
      idx++;
      if (idx == numChildren_) {
        break;
      }
    }
  }
  freeBitMask_ = (1LL << numChildren_) - 1;

  std::memset(node256->nodes_, 0, sizeof(node256->nodes_));
  objectsPool_->Put(
      ::exchange::core::collections::objpool::ObjectsPool::ART_NODE_256,
      node256);
}

template <typename V> V *ArtNode48<V>::GetValue(int64_t key, int level) {
  if (level != nodeLevel_ &&
      ((key ^ nodeKey_) & (-1LL << (nodeLevel_ + 8))) != 0) {
    return nullptr;
  }
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

template <typename V>
IArtNode<V> *ArtNode48<V>::Put(int64_t key, int level, V *value) {
  if (level != nodeLevel_) {
    IArtNode<V> *branch = LongAdaptiveRadixTreeMap<V>::BranchIfRequired(
        key, value, nodeKey_, nodeLevel_, this);
    if (branch != nullptr) {
      return branch;
    }
  }
  const int16_t idx = static_cast<int16_t>((key >> nodeLevel_) & 0xFF);
  const int8_t nodeIndex = indexes_[idx];
  if (nodeIndex != -1) {
    // Found
    if (nodeLevel_ == 0) {
      nodes_[nodeIndex] = value;
    } else {
      IArtNode<V> *oldNode = static_cast<IArtNode<V> *>(nodes_[nodeIndex]);
      IArtNode<V> *resizedNode = oldNode->Put(key, nodeLevel_ - 8, value);
      if (resizedNode != nullptr) {
        LongAdaptiveRadixTreeMap<V>::RecycleNodeToPool(oldNode);
        nodes_[nodeIndex] = resizedNode;
      }
    }
    return nullptr;
  }

  // Not found, put new element
  if (numChildren_ != 48) {
    // Capacity less than 48 - can simply insert node
    const int8_t freePosition =
        static_cast<int8_t>(__builtin_ctzll(~freeBitMask_));
    indexes_[idx] = freePosition;

    if (nodeLevel_ == 0) {
      nodes_[freePosition] = value;
    } else {
      ArtNode4<V> *newSubNode = objectsPool_->Get<ArtNode4<V>>(
          ::exchange::core::collections::objpool::ObjectsPool::ART_NODE_4,
          [this]() { return new ArtNode4<V>(objectsPool_); });
      newSubNode->InitFirstKey(key, value);
      nodes_[freePosition] = newSubNode;
    }
    numChildren_++;
    freeBitMask_ = freeBitMask_ ^ (1LL << freePosition);
    return nullptr;
  } else {
    // No space left, create a ArtNode256 containing a new item
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

    ArtNode256<V> *node256 = objectsPool_->Get<ArtNode256<V>>(
        ::exchange::core::collections::objpool::ObjectsPool::ART_NODE_256,
        [this]() { return new ArtNode256<V>(objectsPool_); });
    node256->InitFromNode48(this, idx, newElement);

    return node256;
  }
}

template <typename V>
IArtNode<V> *ArtNode48<V>::Remove(int64_t key, int level) {
  if (level != nodeLevel_ &&
      ((key ^ nodeKey_) & (-1LL << (nodeLevel_ + 8))) != 0) {
    return this;
  }
  const int16_t idx = static_cast<int16_t>((key >> nodeLevel_) & 0xFF);
  const int8_t nodeIndex = indexes_[idx];
  if (nodeIndex == -1) {
    return this;
  }

  if (nodeLevel_ == 0) {
    nodes_[nodeIndex] = nullptr;
    indexes_[idx] = -1;
    numChildren_--;
    freeBitMask_ = freeBitMask_ ^ (1LL << nodeIndex);
  } else {
    IArtNode<V> *oldNode = static_cast<IArtNode<V> *>(nodes_[nodeIndex]);
    IArtNode<V> *resizedNode = oldNode->Remove(key, nodeLevel_ - 8);
    if (resizedNode != oldNode) {
      LongAdaptiveRadixTreeMap<V>::RecycleNodeToPool(oldNode);
      nodes_[nodeIndex] = resizedNode;
      if (resizedNode == nullptr) {
        numChildren_--;
        indexes_[idx] = -1;
        freeBitMask_ = freeBitMask_ ^ (1LL << nodeIndex);
      }
    }
  }

  if (numChildren_ == NODE16_SWITCH_THRESHOLD) {
    ArtNode16<V> *newNode = objectsPool_->Get<ArtNode16<V>>(
        ::exchange::core::collections::objpool::ObjectsPool::ART_NODE_16,
        [this]() { return new ArtNode16<V>(objectsPool_); });
    newNode->InitFromNode48(this);
    return newNode;
  } else {
    return this;
  }
}

template <typename V> V *ArtNode48<V>::GetCeilingValue(int64_t key, int level) {
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

  int16_t idx = static_cast<int16_t>((key >> nodeLevel_) & 0xFF);

  int8_t index = indexes_[idx];
  if (index != -1) {
    V *res = nodeLevel_ == 0 ? static_cast<V *>(nodes_[index])
                             : static_cast<IArtNode<V> *>(nodes_[index])
                                   ->GetCeilingValue(key, nodeLevel_ - 8);
    if (res != nullptr) {
      return res;
    }
  }

  // If exact key not found - searching for first higher key
  while (++idx < 256) {
    index = indexes_[idx];
    if (index != -1) {
      if (nodeLevel_ == 0) {
        return static_cast<V *>(nodes_[index]);
      } else {
        return static_cast<IArtNode<V> *>(nodes_[index])
            ->GetCeilingValue(0, nodeLevel_ - 8);
      }
    }
  }

  return nullptr;
}

template <typename V> V *ArtNode48<V>::GetFloorValue(int64_t key, int level) {
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

  int16_t idx = static_cast<int16_t>((key >> nodeLevel_) & 0xFF);

  int8_t index = indexes_[idx];
  if (index != -1) {
    V *res = nodeLevel_ == 0 ? static_cast<V *>(nodes_[index])
                             : static_cast<IArtNode<V> *>(nodes_[index])
                                   ->GetFloorValue(key, nodeLevel_ - 8);
    if (res != nullptr) {
      return res;
    }
  }

  // If exact key not found - searching for first lower key
  while (--idx >= 0) {
    index = indexes_[idx];
    if (index != -1) {
      if (nodeLevel_ == 0) {
        return static_cast<V *>(nodes_[index]);
      } else {
        return static_cast<IArtNode<V> *>(nodes_[index])
            ->GetFloorValue(INT64_MAX, nodeLevel_ - 8);
      }
    }
  }

  return nullptr;
}

template <typename V>
int ArtNode48<V>::ForEach(LongObjConsumer<V> *consumer, int limit) {
  if (nodeLevel_ == 0) {
    const int64_t keyBase = (nodeKey_ >> 8) << 8;
    int numFound = 0;
    for (int16_t i = 0; i < 256; i++) {
      if (numFound == limit) {
        return numFound;
      }
      const int8_t index = indexes_[i];
      if (index != -1) {
        consumer->Accept(keyBase + i, static_cast<V *>(nodes_[index]));
        numFound++;
      }
    }
    return numFound;
  } else {
    int numLeft = limit;
    for (int16_t i = 0; i < 256 && numLeft > 0; i++) {
      const int8_t index = indexes_[i];
      if (index != -1) {
        numLeft -= static_cast<IArtNode<V> *>(nodes_[index])
                       ->ForEach(consumer, numLeft);
      }
    }
    return limit - numLeft;
  }
}

template <typename V>
int ArtNode48<V>::ForEachDesc(LongObjConsumer<V> *consumer, int limit) {
  if (nodeLevel_ == 0) {
    const int64_t keyBase = (nodeKey_ >> 8) << 8;
    int numFound = 0;
    for (int16_t i = 255; i >= 0; i--) {
      if (numFound == limit) {
        return numFound;
      }
      const int8_t index = indexes_[i];
      if (index != -1) {
        consumer->Accept(keyBase + i, static_cast<V *>(nodes_[index]));
        numFound++;
      }
    }
    return numFound;
  } else {
    int numLeft = limit;
    for (int16_t i = 255; i >= 0 && numLeft > 0; i--) {
      const int8_t index = indexes_[i];
      if (index != -1) {
        numLeft -= static_cast<IArtNode<V> *>(nodes_[index])
                       ->ForEachDesc(consumer, numLeft);
      }
    }
    return limit - numLeft;
  }
}

template <typename V> int ArtNode48<V>::Size(int limit) {
  if (nodeLevel_ == 0) {
    return numChildren_;
  } else {
    int numLeft = limit;
    for (int16_t i = 0; i < 256 && numLeft > 0; i++) {
      const int8_t index = indexes_[i];
      if (index != -1) {
        numLeft -= static_cast<IArtNode<V> *>(nodes_[index])->Size(numLeft);
      }
    }
    return limit - numLeft;
  }
}

template <typename V> void ArtNode48<V>::ValidateInternalState(int level) {
  if (nodeLevel_ > level)
    throw std::runtime_error("unexpected nodeLevel");
  int found = 0;
  int64_t expectedBitMask = 0;
  for (int i = 0; i < 256; i++) {
    int8_t idx = indexes_[i];
    if (idx != -1) {
      if (idx > 47 || idx < -1)
        throw std::runtime_error("wrong index");
      found++;
      if (nodes_[idx] == nullptr)
        throw std::runtime_error("null node");
      expectedBitMask ^= (1LL << idx);
    }
  }
  if (freeBitMask_ != expectedBitMask)
    throw std::runtime_error("freeBitMask is wrong");
  if (found != numChildren_)
    throw std::runtime_error("wrong numChildren");
  if (numChildren_ > 48 || numChildren_ <= NODE16_SWITCH_THRESHOLD)
    throw std::runtime_error("unexpected numChildren");
}

template <typename V>
std::string ArtNode48<V>::PrintDiagram(const std::string &prefix, int level) {
  std::vector<int16_t> keys = CreateKeysArray();
  return LongAdaptiveRadixTreeMap<V>::PrintDiagram(
      prefix, level, nodeLevel_, nodeKey_, numChildren_,
      [&keys](int idx) { return keys[idx]; },
      [this, &keys](int idx) { return nodes_[indexes_[keys[idx]]]; });
}

template <typename V>
std::list<std::pair<int64_t, V *>> ArtNode48<V>::Entries() {
  const int64_t keyPrefix = nodeKey_ & (-1LL << 8);
  std::list<std::pair<int64_t, V *>> list;
  std::vector<int16_t> keys = CreateKeysArray();
  for (int i = 0; i < numChildren_; i++) {
    if (nodeLevel_ == 0) {
      list.push_back(std::make_pair(
          keyPrefix + keys[i], static_cast<V *>(nodes_[indexes_[keys[i]]])));
    } else {
      auto subEntries =
          static_cast<IArtNode<V> *>(nodes_[indexes_[keys[i]]])->Entries();
      list.splice(list.end(), subEntries);
    }
  }
  return list;
}

template <typename V>
::exchange::core::collections::objpool::ObjectsPool *
ArtNode48<V>::GetObjectsPool() {
  return objectsPool_;
}

template <typename V> std::vector<int16_t> ArtNode48<V>::CreateKeysArray() {
  std::vector<int16_t> keys;
  keys.reserve(numChildren_);
  for (int16_t i = 0; i < 256; i++) {
    if (indexes_[i] != -1) {
      keys.push_back(i);
    }
  }
  return keys;
}

// Explicit template instantiations
template class ArtNode48<void>;

} // namespace art
} // namespace collections
} // namespace core
} // namespace exchange
