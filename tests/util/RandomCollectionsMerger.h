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

#include <cstdint>
#include <vector>

namespace exchange {
namespace core {
namespace tests {
namespace util {

/**
 * RandomCollectionsMerger - merges multiple collections using weighted random
 * distribution
 */
class RandomCollectionsMerger {
public:
  /**
   * Merge multiple collections into one using weighted random distribution
   * @tparam T - element type
   * @param chunks - vector of collections to merge (will be consumed/moved)
   * @param seed - random seed
   * @return merged collection
   */
  template <typename T>
  static std::vector<T> MergeCollections(std::vector<std::vector<T>> &chunks,
                                         int64_t seed);
};

} // namespace util
} // namespace tests
} // namespace core
} // namespace exchange

// Template implementation
#include <algorithm>
#include <random>

namespace exchange {
namespace core {
namespace tests {
namespace util {

template <typename T>
std::vector<T>
RandomCollectionsMerger::MergeCollections(std::vector<std::vector<T>> &chunks,
                                          int64_t seed) {
  std::mt19937 rng(static_cast<uint32_t>(std::hash<int64_t>{}(seed)));

  std::vector<T> mergedResult;

  // Create working copies with indices
  // Use reference to avoid copying non-copyable types
  struct ChunkInfo {
    std::vector<T> *data; // Pointer to allow moving
    size_t currentIndex;
    size_t size;

    ChunkInfo(std::vector<T> &chunk)
        : data(&chunk), currentIndex(0), size(chunk.size()) {}
  };

  std::vector<ChunkInfo> activeChunks;
  activeChunks.reserve(chunks.size());
  for (auto &chunk : chunks) {
    if (!chunk.empty()) {
      activeChunks.emplace_back(chunk);
    }
  }

  while (!activeChunks.empty()) {
    // Calculate total weight (sum of remaining sizes)
    size_t totalWeight = 0;
    for (const auto &chunk : activeChunks) {
      totalWeight += (chunk.size - chunk.currentIndex);
    }

    if (totalWeight == 0) {
      break;
    }

    // Try to sample from active chunks (weighted by remaining size)
    int missCounter = 0;
    while (missCounter < 3 && !activeChunks.empty()) {
      // Generate random number in [0, totalWeight)
      if (totalWeight == 0) {
        break;
      }
      std::uniform_int_distribution<size_t> dist(0, totalWeight - 1);
      size_t randomValue = dist(rng);

      // Find which chunk this random value falls into
      size_t cumulativeWeight = 0;
      size_t selectedIndex = 0;
      for (size_t i = 0; i < activeChunks.size(); i++) {
        size_t chunkWeight =
            activeChunks[i].size - activeChunks[i].currentIndex;
        cumulativeWeight += chunkWeight;
        if (randomValue < cumulativeWeight) {
          selectedIndex = i;
          break;
        }
      }

      // Try to take element from selected chunk
      auto &selectedChunk = activeChunks[selectedIndex];
      if (selectedChunk.currentIndex < selectedChunk.size) {
        // Use move semantics for non-copyable types
        mergedResult.push_back(
            std::move((*selectedChunk.data)[selectedChunk.currentIndex]));
        selectedChunk.currentIndex++;
        missCounter = 0;
        totalWeight--;
      } else {
        missCounter++;
      }
    }

    // Remove empty chunks
    activeChunks.erase(std::remove_if(activeChunks.begin(), activeChunks.end(),
                                      [](const ChunkInfo &chunk) {
                                        return chunk.currentIndex >= chunk.size;
                                      }),
                       activeChunks.end());
  }

  return mergedResult;
}

} // namespace util
} // namespace tests
} // namespace core
} // namespace exchange
