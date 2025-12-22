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
template <typename V> class ArtNode48;

/**
 * ArtNode256 - The largest node type is simply an array of 256 pointers and is
 * used for storing between 49 and 256 entries. With this representation, the
 * next node can be found very efficiently using a single lookup of the key byte
 * in that array. No additional indirection is necessary. If most entries are
 * not null, this representation is also very space efficient because only
 * pointers need to be stored.
 *
 * @tparam V Value type
 */
template <typename V> class ArtNode256 : public IArtNode<V> {
public:
  static constexpr int NODE48_SWITCH_THRESHOLD = 37;

  explicit ArtNode256(objpool::ObjectsPool *objectsPool);

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
   * Initialize from Node48 (upsize 48->256)
   */
  void InitFromNode48(ArtNode48<V> *node48, int16_t subKey, void *newElement);

private:
  // Direct addressing - 256 pointers
  void *nodes_[256]; // IArtNode<V>* or V* (if nodeLevel == 0)

  objpool::ObjectsPool *objectsPool_;

  int64_t nodeKey_;
  int nodeLevel_;
  int16_t numChildren_;

  int16_t *CreateKeysArray();
};

} // namespace art
} // namespace collections
} // namespace core
} // namespace exchange
