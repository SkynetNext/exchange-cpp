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
ArtNode256<V>::ArtNode256(
    ::exchange::core::collections::objpool::ObjectsPool *objectsPool)
    : objectsPool_(objectsPool), nodeKey_(0), nodeLevel_(0), numChildren_(0) {
  std::memset(nodes_, 0, sizeof(nodes_));
}

template <typename V>
void ArtNode256<V>::InitFromNode48(ArtNode48<V> *artNode48, int16_t subKey,
                                   void *newElement) {
  nodeLevel_ = artNode48->nodeLevel_;
  nodeKey_ = artNode48->nodeKey_;
  const int sourceSize = 48;
  for (int16_t i = 0; i < 256; i++) {
    const int8_t index = artNode48->indexes_[i];
    if (index != -1) {
      nodes_[i] = artNode48->nodes_[index];
    }
  }
  nodes_[subKey] = newElement;
  numChildren_ = sourceSize + 1;

  std::memset(artNode48->nodes_, 0, sizeof(artNode48->nodes_));
  std::memset(artNode48->indexes_, -1, sizeof(artNode48->indexes_));
  objectsPool_->Put(
      ::exchange::core::collections::objpool::ObjectsPool::ART_NODE_48,
      artNode48);
}

template <typename V> V *ArtNode256<V>::GetValue(int64_t key, int level) {
  if (level != nodeLevel_ &&
      ((key ^ nodeKey_) & (-1LL << (nodeLevel_ + 8))) != 0) {
    return nullptr;
  }
  const int16_t idx = static_cast<int16_t>((key >> nodeLevel_) & 0xFF);
  void *node = nodes_[idx];
  if (node != nullptr) {
    return nodeLevel_ == 0 ? static_cast<V *>(node)
                           : static_cast<IArtNode<V> *>(node)->GetValue(
                                 key, nodeLevel_ - 8);
  }
  return nullptr;
}

template <typename V>
IArtNode<V> *ArtNode256<V>::Put(int64_t key, int level, V *value) {
  if (level != nodeLevel_) {
    IArtNode<V> *branch = LongAdaptiveRadixTreeMap<V>::BranchIfRequired(
        key, value, nodeKey_, nodeLevel_, this);
    if (branch != nullptr) {
      return branch;
    }
  }
  const int16_t idx = static_cast<int16_t>((key >> nodeLevel_) & 0xFF);
  if (nodes_[idx] == nullptr) {
    // New object will be inserted
    numChildren_++;
  }

  if (nodeLevel_ == 0) {
    nodes_[idx] = value;
  } else {
    IArtNode<V> *node = static_cast<IArtNode<V> *>(nodes_[idx]);
    if (node != nullptr) {
      IArtNode<V> *oldNode = node;
      IArtNode<V> *resizedNode = oldNode->Put(key, nodeLevel_ - 8, value);
      if (resizedNode != nullptr) {
        LongAdaptiveRadixTreeMap<V>::RecycleNodeToPool(oldNode);
        nodes_[idx] = resizedNode;
      }
    } else {
      ArtNode4<V> *newSubNode = objectsPool_->Get<ArtNode4<V>>(
          ::exchange::core::collections::objpool::ObjectsPool::ART_NODE_4,
          [this]() { return new ArtNode4<V>(objectsPool_); });
      newSubNode->InitFirstKey(key, value);
      nodes_[idx] = newSubNode;
    }
  }

  // Never need to increase the size
  return nullptr;
}

template <typename V>
IArtNode<V> *ArtNode256<V>::Remove(int64_t key, int level) {
  if (level != nodeLevel_ &&
      ((key ^ nodeKey_) & (-1LL << (nodeLevel_ + 8))) != 0) {
    return this;
  }
  const int16_t idx = static_cast<int16_t>((key >> nodeLevel_) & 0xFF);
  if (nodes_[idx] == nullptr) {
    return this;
  }

  if (nodeLevel_ == 0) {
    nodes_[idx] = nullptr;
    numChildren_--;
  } else {
    IArtNode<V> *node = static_cast<IArtNode<V> *>(nodes_[idx]);
    IArtNode<V> *oldNode = node;
    IArtNode<V> *resizedNode = oldNode->Remove(key, nodeLevel_ - 8);
    if (resizedNode != oldNode) {
      LongAdaptiveRadixTreeMap<V>::RecycleNodeToPool(oldNode);
      nodes_[idx] = resizedNode;
      if (resizedNode == nullptr) {
        numChildren_--;
      }
    }
  }

  if (numChildren_ == NODE48_SWITCH_THRESHOLD) {
    ArtNode48<V> *newNode = objectsPool_->Get<ArtNode48<V>>(
        ::exchange::core::collections::objpool::ObjectsPool::ART_NODE_48,
        [this]() { return new ArtNode48<V>(objectsPool_); });
    newNode->InitFromNode256(this);
    return newNode;
  } else {
    return this;
  }
}

template <typename V>
V *ArtNode256<V>::GetCeilingValue(int64_t key, int level) {
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
  void *node = nodes_[idx];
  if (node != nullptr) {
    V *res = nodeLevel_ == 0
                 ? static_cast<V *>(node)
                 : static_cast<IArtNode<V> *>(node)->GetCeilingValue(
                       key, nodeLevel_ - 8);
    if (res != nullptr) {
      return res;
    }
  }

  // If exact key not found - searching for first higher key
  while (++idx < 256) {
    node = nodes_[idx];
    if (node != nullptr) {
      return (nodeLevel_ == 0)
                 ? static_cast<V *>(node)
                 : static_cast<IArtNode<V> *>(node)->GetCeilingValue(
                       0, nodeLevel_ - 8);
    }
  }
  return nullptr;
}

template <typename V> V *ArtNode256<V>::GetFloorValue(int64_t key, int level) {
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
  void *node = nodes_[idx];
  if (node != nullptr) {
    V *res = nodeLevel_ == 0 ? static_cast<V *>(node)
                             : static_cast<IArtNode<V> *>(node)->GetFloorValue(
                                   key, nodeLevel_ - 8);
    if (res != nullptr) {
      return res;
    }
  }

  // If exact key not found - searching for first lower key
  while (--idx >= 0) {
    node = nodes_[idx];
    if (node != nullptr) {
      return (nodeLevel_ == 0)
                 ? static_cast<V *>(node)
                 : static_cast<IArtNode<V> *>(node)->GetFloorValue(
                       INT64_MAX, nodeLevel_ - 8);
    }
  }
  return nullptr;
}

template <typename V>
int ArtNode256<V>::ForEach(LongObjConsumer<V> *consumer, int limit) {
  if (nodeLevel_ == 0) {
    const int64_t keyBase = (nodeKey_ >> 8) << 8;
    int numFound = 0;
    for (int16_t i = 0; i < 256; i++) {
      if (numFound == limit) {
        return numFound;
      }
      V *node = static_cast<V *>(nodes_[i]);
      if (node != nullptr) {
        consumer->Accept(keyBase + i, node);
        numFound++;
      }
    }
    return numFound;
  } else {
    int numLeft = limit;
    for (int16_t i = 0; i < 256 && numLeft > 0; i++) {
      IArtNode<V> *node = static_cast<IArtNode<V> *>(nodes_[i]);
      if (node != nullptr) {
        numLeft -= node->ForEach(consumer, numLeft);
      }
    }
    return limit - numLeft;
  }
}

template <typename V>
int ArtNode256<V>::ForEachDesc(LongObjConsumer<V> *consumer, int limit) {
  if (nodeLevel_ == 0) {
    const int64_t keyBase = (nodeKey_ >> 8) << 8;
    int numFound = 0;
    for (int16_t i = 255; i >= 0; i--) {
      if (numFound == limit) {
        return numFound;
      }
      V *node = static_cast<V *>(nodes_[i]);
      if (node != nullptr) {
        consumer->Accept(keyBase + i, node);
        numFound++;
      }
    }
    return numFound;
  } else {
    int numLeft = limit;
    for (int16_t i = 255; i >= 0 && numLeft > 0; i--) {
      IArtNode<V> *node = static_cast<IArtNode<V> *>(nodes_[i]);
      if (node != nullptr) {
        numLeft -= node->ForEachDesc(consumer, numLeft);
      }
    }
    return limit - numLeft;
  }
}

template <typename V> int ArtNode256<V>::Size(int limit) {
  if (nodeLevel_ == 0) {
    return numChildren_;
  } else {
    int numLeft = limit;
    for (int16_t i = 0; i < 256 && numLeft > 0; i++) {
      IArtNode<V> *node = static_cast<IArtNode<V> *>(nodes_[i]);
      if (node != nullptr) {
        numLeft -= node->Size(numLeft);
      }
    }
    return limit - numLeft;
  }
}

template <typename V> void ArtNode256<V>::ValidateInternalState(int level) {
  if (nodeLevel_ > level)
    throw std::runtime_error("unexpected nodeLevel");
  int found = 0;
  for (int i = 0; i < 256; i++) {
    void *node = nodes_[i];
    if (node != nullptr) {
      if (nodeLevel_ != 0) {
        IArtNode<V> *artNode = static_cast<IArtNode<V> *>(node);
        artNode->ValidateInternalState(nodeLevel_ - 8);
      }
      found++;
    }
  }
  if (found != numChildren_) {
    throw std::runtime_error("wrong numChildren");
  }
  if (numChildren_ <= NODE48_SWITCH_THRESHOLD || numChildren_ > 256) {
    throw std::runtime_error("unexpected numChildren");
  }
}

template <typename V>
std::string ArtNode256<V>::PrintDiagram(const std::string &prefix, int level) {
  std::vector<int16_t> keys = CreateKeysArray();
  return LongAdaptiveRadixTreeMap<V>::PrintDiagram(
      prefix, level, nodeLevel_, nodeKey_, numChildren_,
      [&keys](int idx) { return keys[idx]; },
      [this, &keys](int idx) { return nodes_[keys[idx]]; });
}

template <typename V>
std::list<std::pair<int64_t, V *>> ArtNode256<V>::Entries() {
  const int64_t keyPrefix = nodeKey_ & (-1LL << 8);
  std::list<std::pair<int64_t, V *>> list;
  std::vector<int16_t> keys = CreateKeysArray();
  for (int i = 0; i < numChildren_; i++) {
    if (nodeLevel_ == 0) {
      list.push_back(std::make_pair(keyPrefix + keys[i],
                                    static_cast<V *>(nodes_[keys[i]])));
    } else {
      auto subEntries = static_cast<IArtNode<V> *>(nodes_[keys[i]])->Entries();
      list.splice(list.end(), subEntries);
    }
  }
  return list;
}

template <typename V>
::exchange::core::collections::objpool::ObjectsPool *
ArtNode256<V>::GetObjectsPool() {
  return objectsPool_;
}

template <typename V> std::vector<int16_t> ArtNode256<V>::CreateKeysArray() {
  std::vector<int16_t> keys;
  keys.reserve(numChildren_);
  for (int16_t i = 0; i < 256; i++) {
    if (nodes_[i] != nullptr) {
      keys.push_back(i);
    }
  }
  return keys;
}

// Explicit template instantiations
template class ArtNode256<void>;

} // namespace art
} // namespace collections
} // namespace core
} // namespace exchange
