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

#include <exchange/core/utils/SerializationUtils.h>

namespace exchange {
namespace core {
namespace utils {

ankerl::unordered_dense::map<int32_t, int64_t> SerializationUtils::MergeSum(
    const ankerl::unordered_dense::map<int32_t, int64_t> *map1,
    const ankerl::unordered_dense::map<int32_t, int64_t> *map2) {
  ankerl::unordered_dense::map<int32_t, int64_t> result;
  if (map1 != nullptr) {
    for (const auto &pair : *map1) {
      result[pair.first] = pair.second;
    }
  }
  if (map2 != nullptr) {
    for (const auto &pair : *map2) {
      result[pair.first] += pair.second;
    }
  }
  return result;
}

ankerl::unordered_dense::map<int32_t, int64_t> SerializationUtils::MergeSum(
    const ankerl::unordered_dense::map<int32_t, int64_t> *map1,
    const ankerl::unordered_dense::map<int32_t, int64_t> *map2,
    const ankerl::unordered_dense::map<int32_t, int64_t> *map3) {
  auto result = MergeSum(map1, map2);
  if (map3 != nullptr) {
    for (const auto &pair : *map3) {
      result[pair.first] += pair.second;
    }
  }
  return result;
}

ankerl::unordered_dense::map<int32_t, int64_t> SerializationUtils::MergeSum(
    const ankerl::unordered_dense::map<int32_t, int64_t> *map1,
    const ankerl::unordered_dense::map<int32_t, int64_t> *map2,
    const ankerl::unordered_dense::map<int32_t, int64_t> *map3,
    const ankerl::unordered_dense::map<int32_t, int64_t> *map4) {
  auto result = MergeSum(map1, map2, map3);
  if (map4 != nullptr) {
    for (const auto &pair : *map4) {
      result[pair.first] += pair.second;
    }
  }
  return result;
}

int SerializationUtils::RequiredLongArraySize(int bytesLength) {
  return ((bytesLength - 1) >> 3) + 1;
}

int SerializationUtils::RequiredLongArraySize(int bytesLength, int padding) {
  int len = RequiredLongArraySize(bytesLength);
  if (padding == 1) {
    return len;
  } else {
    int rem = len % padding;
    return rem == 0 ? len : (len + padding - rem);
  }
}

void SerializationUtils::MarshallLongArray(const std::vector<int64_t> &longs,
                                           common::BytesOut &bytes) {
  bytes.WriteInt(static_cast<int32_t>(longs.size()));
  for (int64_t word : longs) {
    bytes.WriteLong(word);
  }
}

std::vector<int64_t> SerializationUtils::ReadLongArray(common::BytesIn &bytes) {
  const int length = bytes.ReadInt();
  std::vector<int64_t> array;
  array.reserve(length);
  for (int i = 0; i < length; i++) {
    array.push_back(bytes.ReadLong());
  }
  return array;
}

void SerializationUtils::MarshallIntLongHashMap(
    const ankerl::unordered_dense::map<int32_t, int64_t> &hashMap,
    common::BytesOut &bytes) {
  bytes.WriteInt(static_cast<int32_t>(hashMap.size()));
  for (const auto &pair : hashMap) {
    bytes.WriteInt(pair.first);
    bytes.WriteLong(pair.second);
  }
}

ankerl::unordered_dense::map<int32_t, int64_t>
SerializationUtils::ReadIntLongHashMap(common::BytesIn &bytes) {
  int length = bytes.ReadInt();
  ankerl::unordered_dense::map<int32_t, int64_t> hashMap;
  hashMap.reserve(length);
  for (int i = 0; i < length; i++) {
    int32_t k = bytes.ReadInt();
    int64_t v = bytes.ReadLong();
    hashMap[k] = v;
  }
  return hashMap;
}

} // namespace utils
} // namespace core
} // namespace exchange
