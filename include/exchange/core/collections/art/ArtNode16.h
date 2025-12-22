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
template <typename V> class ArtNode4;
template <typename V> class ArtNode48;

/**
 * ArtNode16 - This node type is used for storing between 5 and 16 child
 * pointers. Like the Node4, the keys and pointers are stored in separate arrays
 * at corresponding positions, but both arrays have space for 16 entries.
 *
 * @tparam V Value type
 */
template <typename V> class ArtNode16 : public IArtNode<V> {
public:
  static constexpr int NODE4_SWITCH_THRESHOLD = 3;

  explicit ArtNode16(
      ::exchange::core::collections::objpool::ObjectsPool *objectsPool);

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
  ::exchange::core::collections::objpool::ObjectsPool *
  GetObjectsPool() override;

  // Friend declarations for access to private members
  template <typename U> friend class ArtNode4;
  template <typename U> friend class ArtNode48;

  /**
   * Initialize from Node4 (upsize 4->16)
   */
  void InitFromNode4(ArtNode4<V> *node4, int16_t subKey, void *newElement);

  /**
   * Initialize from Node48 (downsize 48->16)
   */
  void InitFromNode48(ArtNode48<V> *node48);

private:
  // Keys are ordered
  int16_t keys_[16];
  void *nodes_[16]; // IArtNode<V>* or V* (if nodeLevel == 0)

  ::exchange::core::collections::objpool::ObjectsPool *objectsPool_;

  int64_t nodeKey_;
  int nodeLevel_;
  uint8_t numChildren_;

  void RemoveElementAtPos(int pos);
};

} // namespace art
} // namespace collections
} // namespace core
} // namespace exchange
