/*
 * Copyright 2019 Maksim Zheravin
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

#include "../common/BytesIn.h"
#include "../common/BytesOut.h"
#include <ankerl/unordered_dense.h>
#include <cstdint>
#include <functional>
#include <map>
#include <vector>

namespace exchange {
namespace core {
namespace utils {

/**
 * SerializationUtils - serialization utility functions
 * 1:1 translation from Java version
 */
class SerializationUtils {
public:
  /**
   * Merge sum of two maps (currency -> amount)
   */
  static ankerl::unordered_dense::map<int32_t, int64_t>
  MergeSum(const ankerl::unordered_dense::map<int32_t, int64_t> *map1,
           const ankerl::unordered_dense::map<int32_t, int64_t> *map2);

  /**
   * Merge sum of multiple maps
   */
  static ankerl::unordered_dense::map<int32_t, int64_t>
  MergeSum(const ankerl::unordered_dense::map<int32_t, int64_t> *map1,
           const ankerl::unordered_dense::map<int32_t, int64_t> *map2,
           const ankerl::unordered_dense::map<int32_t, int64_t> *map3);

  /**
   * Merge sum of multiple maps (4 maps)
   */
  static ankerl::unordered_dense::map<int32_t, int64_t>
  MergeSum(const ankerl::unordered_dense::map<int32_t, int64_t> *map1,
           const ankerl::unordered_dense::map<int32_t, int64_t> *map2,
           const ankerl::unordered_dense::map<int32_t, int64_t> *map3,
           const ankerl::unordered_dense::map<int32_t, int64_t> *map4);

  /**
   * Prefer non-null value
   */
  template <typename T> static T *PreferNotNull(T *a, T *b) {
    return a != nullptr ? a : b;
  }

  /**
   * Merge override - merge two maps, b overrides a
   */
  template <typename K, typename V>
  static ankerl::unordered_dense::map<K, V>
  MergeOverride(const ankerl::unordered_dense::map<K, V> *a,
                const ankerl::unordered_dense::map<K, V> *b) {
    ankerl::unordered_dense::map<K, V> result;
    if (a != nullptr) {
      result = *a;
    }
    if (b != nullptr) {
      for (const auto &pair : *b) {
        result[pair.first] = pair.second;
      }
    }
    return result;
  }

  // Serialization methods - 1:1 translation from Java

  /**
   * Calculate required long array size for bytes
   */
  static int RequiredLongArraySize(int bytesLength);

  /**
   * Calculate required long array size with padding
   */
  static int RequiredLongArraySize(int bytesLength, int padding);

  /**
   * Marshall long array
   */
  static void MarshallLongArray(const std::vector<int64_t> &longs,
                                common::BytesOut &bytes);

  /**
   * Read long array
   */
  static std::vector<int64_t> ReadLongArray(common::BytesIn &bytes);

  /**
   * Marshall int->long hash map
   */
  static void MarshallIntLongHashMap(
      const ankerl::unordered_dense::map<int32_t, int64_t> &hashMap,
      common::BytesOut &bytes);

  /**
   * Read int->long hash map
   */
  static ankerl::unordered_dense::map<int32_t, int64_t>
  ReadIntLongHashMap(common::BytesIn &bytes);

  /**
   * Marshall int->Object hash map (with WriteBytesMarshallable values)
   */
  template <typename T>
  static void
  MarshallIntHashMap(const ankerl::unordered_dense::map<int32_t, T *> &hashMap,
                     common::BytesOut &bytes) {
    bytes.WriteInt(static_cast<int32_t>(hashMap.size()));
    for (const auto &pair : hashMap) {
      bytes.WriteInt(pair.first);
      if (pair.second != nullptr) {
        pair.second->WriteMarshallable(bytes);
      }
    }
  }

  /**
   * Marshall int->Object hash map (with custom marshaller)
   */
  template <typename T>
  static void MarshallIntHashMap(
      const ankerl::unordered_dense::map<int32_t, T *> &hashMap,
      common::BytesOut &bytes,
      std::function<void(T *, common::BytesOut &)> elementMarshaller) {
    bytes.WriteInt(static_cast<int32_t>(hashMap.size()));
    for (const auto &pair : hashMap) {
      bytes.WriteInt(pair.first);
      if (pair.second != nullptr) {
        elementMarshaller(pair.second, bytes);
      }
    }
  }

  /**
   * Read int->Object hash map
   */
  template <typename T>
  static ankerl::unordered_dense::map<int32_t, T *>
  ReadIntHashMap(common::BytesIn &bytes,
                 std::function<T *(common::BytesIn &)> creator) {
    int length = bytes.ReadInt();
    ankerl::unordered_dense::map<int32_t, T *> hashMap;
    hashMap.reserve(length);
    for (int i = 0; i < length; i++) {
      int32_t key = bytes.ReadInt();
      T *value = creator(bytes);
      hashMap[key] = value;
    }
    return hashMap;
  }

  /**
   * Marshall long->Object hash map (with WriteBytesMarshallable values)
   */
  template <typename T>
  static void
  MarshallLongHashMap(const ankerl::unordered_dense::map<int64_t, T *> &hashMap,
                      common::BytesOut &bytes) {
    bytes.WriteInt(static_cast<int32_t>(hashMap.size()));
    for (const auto &pair : hashMap) {
      bytes.WriteLong(pair.first);
      if (pair.second != nullptr) {
        pair.second->WriteMarshallable(bytes);
      }
    }
  }

  /**
   * Read long->Object hash map
   */
  template <typename T>
  static ankerl::unordered_dense::map<int64_t, T *>
  ReadLongHashMap(common::BytesIn &bytes,
                  std::function<T *(common::BytesIn &)> creator) {
    int length = bytes.ReadInt();
    ankerl::unordered_dense::map<int64_t, T *> hashMap;
    hashMap.reserve(length);
    for (int i = 0; i < length; i++) {
      int64_t key = bytes.ReadLong();
      T *value = creator(bytes);
      hashMap[key] = value;
    }
    return hashMap;
  }

  /**
   * Marshall nullable object
   */
  template <typename T>
  static void
  MarshallNullable(T *object, common::BytesOut &bytes,
                   std::function<void(T *, common::BytesOut &)> marshaller) {
    bytes.WriteBoolean(object != nullptr);
    if (object != nullptr) {
      marshaller(object, bytes);
    }
  }

  /**
   * Read nullable object
   */
  template <typename T>
  static T *ReadNullable(common::BytesIn &bytesIn,
                         std::function<T *(common::BytesIn &)> creator) {
    return bytesIn.ReadBoolean() ? creator(bytesIn) : nullptr;
  }

  /**
   * Convert long array with LZ4 compressed data to bytes (decompress)
   * Match Java: SerializationUtils.longsLz4ToWire()
   *
   * @param dataArray Long array containing compressed data
   * @param longsTransferred Number of longs actually used
   * @return Vector of decompressed bytes
   */
  static std::vector<uint8_t>
  LongsLz4ToBytes(const std::vector<int64_t> &dataArray, int longsTransferred);

  /**
   * Convert bytes to long array with LZ4 compression
   * Match Java: SerializationUtils.bytesToLongArrayLz4()
   *
   * @param bytes Bytes to compress
   * @param padding Padding for long array size (LONGS_PER_MESSAGE)
   * @return Vector of int64_t containing compressed data
   */
  static std::vector<int64_t>
  BytesToLongArrayLz4(const std::vector<uint8_t> &bytes, int padding);

  /**
   * Convert bytes to long array with padding
   * Match Java: SerializationUtils.bytesToLongArray()
   *
   * @param bytes Bytes to convert
   * @param padding Padding for long array size (LONGS_PER_MESSAGE)
   * @return Vector of int64_t containing the data
   */
  static std::vector<int64_t>
  BytesToLongArray(const std::vector<uint8_t> &bytes, int padding);

  /**
   * Convert long array to bytes (matches Java SerializationUtils.longsToWire)
   * @param longs Vector of int64_t to convert
   * @return Vector of bytes
   */
  static std::vector<uint8_t> LongsToBytes(const std::vector<int64_t> &longs);

  /**
   * Marshall generic map
   * Match Java: SerializationUtils.marshallGenericMap()
   */
  template <typename K, typename V>
  static void MarshallGenericMap(
      const std::map<K, V> &map, common::BytesOut &bytes,
      std::function<void(common::BytesOut &, const K &)> keyMarshaller,
      std::function<void(common::BytesOut &, const V &)> valMarshaller) {
    bytes.WriteInt(static_cast<int32_t>(map.size()));
    for (const auto &pair : map) {
      keyMarshaller(bytes, pair.first);
      valMarshaller(bytes, pair.second);
    }
  }

  /**
   * Marshall nullable object
   * Match Java: SerializationUtils.marshallNullable()
   */
  template <typename T>
  static void MarshallNullable(
      const T *object, common::BytesOut &bytes,
      std::function<void(const T *, common::BytesOut &)> marshaller) {
    bytes.WriteBoolean(object != nullptr);
    if (object != nullptr) {
      marshaller(object, bytes);
    }
  }

  /**
   * Marshall list
   * Match Java: SerializationUtils.marshallList()
   */
  template <typename T>
  static void
  MarshallList(const std::vector<T *> &list, common::BytesOut &bytes,
               std::function<void(T *, common::BytesOut &)> elementMarshaller) {
    bytes.WriteInt(static_cast<int32_t>(list.size()));
    for (T *element : list) {
      if (element != nullptr) {
        elementMarshaller(element, bytes);
      }
    }
  }
};

} // namespace utils
} // namespace core
} // namespace exchange
