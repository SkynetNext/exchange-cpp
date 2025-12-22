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
template <typename V> class ArtNode256;

/**
 * ArtNode48 - As the number of entries in a node increases, searching the key
 * array becomes expensive. Therefore, nodes with more than 16 pointers do not
 * store the keys explicitly. Instead, a 256-element array is used, which can be
 * indexed with key bytes directly. If a node has between 17 and 48 child
 * pointers, this array stores indexes into a second array which contains up to
 * 48 pointers. This indirection saves space in comparison to 256 pointers of 8
 * bytes, because the indexes only require 6 bits (we use 1 byte for
 * simplicity).
 *
 * @tparam V Value type
 */
template <typename V> class ArtNode48 : public IArtNode<V> {
public:
  static constexpr int NODE16_SWITCH_THRESHOLD = 12;

  explicit ArtNode48(objpool::ObjectsPool *objectsPool);

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
   * Initialize from Node16 (upsize 16->48)
   */
  void InitFromNode16(ArtNode16<V> *node16, int16_t subKey, void *newElement);

  /**
   * Initialize from Node256 (downsize 256->48)
   */
  void InitFromNode256(ArtNode256<V> *node256);

private:
  // Index mapping: key byte -> child index (0-47), -1 if not present
  int8_t indexes_[256];
  void *nodes_[48]; // IArtNode<V>* or V* (if nodeLevel == 0)

  objpool::ObjectsPool *objectsPool_;

  int64_t nodeKey_;
  int nodeLevel_;
  uint8_t numChildren_;
  int64_t freeBitMask_; // Bit mask for free positions

  int16_t *CreateKeysArray();
};

} // namespace art
} // namespace collections
} // namespace core
} // namespace exchange
