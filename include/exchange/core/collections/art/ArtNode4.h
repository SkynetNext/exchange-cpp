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
#include <list>
#include <string>

namespace exchange {
namespace core {
namespace collections {
namespace art {

// Forward declarations
namespace objpool {
class ObjectsPool;
}
template <typename V> class ArtNode16;

/**
 * ArtNode4 - The smallest node type can store up to 4 child pointers
 * Keys and pointers are stored at corresponding positions and the keys are
 * sorted.
 *
 * @tparam V Value type
 */
template <typename V> class ArtNode4 : public IArtNode<V> {
public:
  explicit ArtNode4(objpool::ObjectsPool *objectsPool);

  // IArtNode interface
  V *GetValue(int64_t key, int level) override;
  IArtNode<V> *Put(int64_t key, int level, V *value) override;
  IArtNode<V> *Remove(int64_t key, int level) override;
  V *GetCeilingValue(int64_t key, int level) override;
  V *GetFloorValue(int64_t key, int level) override;
  int ForEach(LongObjConsumer<V> *consumer, int limit) override;
  int ForEachDesc(LongObjConsumer<V> *consumer, int limit) override;
  int Size(int limit) override;
  void ValidateInternalState(int level) override;
  std::string PrintDiagram(const std::string &prefix, int level) override;
  std::list<std::pair<int64_t, V *>> Entries() override;
  objpool::ObjectsPool *GetObjectsPool() override;

  /**
   * Initialize with first key (terminal node, nodeLevel=0)
   */
  void InitFirstKey(int64_t key, V *value);

  /**
   * Initialize with two keys (split-compact operation)
   */
  void InitTwoKeys(int64_t key1, void *value1, int64_t key2, void *value2,
                   int level);

  /**
   * Initialize from Node16 (downsize 16->4)
   */
  void InitFromNode16(ArtNode16<V> *node16);

  // Friend declarations for access to private members
  template <typename U> friend class ArtNode16;

private:
  // Keys are ordered
  int16_t keys_[4];
  void *nodes_[4]; // IArtNode<V>* or V* (if nodeLevel == 0)

  objpool::ObjectsPool *objectsPool_;

  int64_t nodeKey_;
  int nodeLevel_;
  uint8_t numChildren_;

  void RemoveElementAtPos(int pos);
};

} // namespace art
} // namespace collections
} // namespace core
} // namespace exchange
