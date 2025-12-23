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

#include <cstring>
#include <exchange/core/utils/SerializationUtils.h>
#include <lz4.h>
#include <stdexcept>

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

std::vector<uint8_t>
SerializationUtils::LongsLz4ToBytes(const std::vector<int64_t> &dataArray,
                                    int longsTransferred) {
  // Convert long array to byte array
  // Each long is 8 bytes, so total bytes = longsTransferred * 8
  const int compressedSizeBytes = longsTransferred * 8;
  std::vector<uint8_t> compressedData(compressedSizeBytes);

  // Copy long array to byte array (little-endian)
  std::memcpy(compressedData.data(), dataArray.data(), compressedSizeBytes);

  // Read original size from first 4 bytes (as int32_t)
  int32_t originalSizeBytes = 0;
  std::memcpy(&originalSizeBytes, compressedData.data(), sizeof(int32_t));

  // Allocate buffer for decompressed data
  std::vector<uint8_t> decompressedData(originalSizeBytes);

  // Decompress using LZ4
  // Skip first 4 bytes (original size) when decompressing
  const int compressedDataSize = compressedSizeBytes - sizeof(int32_t);
  const int decompressedSize = LZ4_decompress_safe(
      reinterpret_cast<const char *>(compressedData.data() + sizeof(int32_t)),
      reinterpret_cast<char *>(decompressedData.data()), compressedDataSize,
      originalSizeBytes);

  if (decompressedSize < 0 || decompressedSize != originalSizeBytes) {
    throw std::runtime_error(
        "LZ4 decompression failed: invalid compressed data or size mismatch");
  }

  return decompressedData;
}

std::vector<int64_t>
SerializationUtils::BytesToLongArrayLz4(const std::vector<uint8_t> &bytes,
                                        int padding) {
  const int originalSize = static_cast<int>(bytes.size());

  // Allocate buffer for compressed data: 4 bytes (original size) + compressed
  // data
  const int maxCompressedSize = LZ4_compressBound(originalSize);
  std::vector<uint8_t> compressedBuffer(4 + maxCompressedSize);

  // Write original size in first 4 bytes
  std::memcpy(compressedBuffer.data(), &originalSize, sizeof(int32_t));

  // Compress using LZ4
  const int compressedSize = LZ4_compress_default(
      reinterpret_cast<const char *>(bytes.data()),
      reinterpret_cast<char *>(compressedBuffer.data() + 4), originalSize,
      maxCompressedSize);

  if (compressedSize <= 0) {
    throw std::runtime_error("LZ4 compression failed");
  }

  // Resize buffer to actual compressed size (including 4-byte header)
  compressedBuffer.resize(4 + compressedSize);

  // Calculate required long array size with padding
  const int longArraySize =
      RequiredLongArraySize(static_cast<int>(compressedBuffer.size()), padding);

  // Convert compressed bytes to long array
  std::vector<int64_t> longArray(longArraySize, 0);
  std::memcpy(longArray.data(), compressedBuffer.data(),
              compressedBuffer.size());

  return longArray;
}

} // namespace utils
} // namespace core
} // namespace exchange
