/*
 * Copyright 2025 Justin Zhu
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

#include <algorithm>
#include <ankerl/unordered_dense.h>
#include <benchmark/benchmark.h>
#include <chrono>
#include <cstdint>
#include <exchange/core/collections/art/LongAdaptiveRadixTreeMap.h>
#include <exchange/core/collections/art/LongObjConsumer.h>
#include <exchange/core/collections/objpool/ObjectsPool.h>
#include <map>
// On Windows, mimalloc-static doesn't automatically override C++ new/delete,
// so we need to include mimalloc-new-delete.h explicitly.
// On Linux/Unix, mimalloc-static automatically overrides via linking.
#ifdef _WIN32
#include <mimalloc-new-delete.h>
#endif
#include <random>
#include <unordered_map>
#include <vector>

// Cross-platform leading zero count
#ifdef _MSC_VER
#include <intrin.h>
inline int CountLeadingZeros64(uint64_t x) {
  unsigned long index;
  return _BitScanReverse64(&index, x) ? (63 - static_cast<int>(index)) : 64;
}
#else
inline int CountLeadingZeros64(uint64_t x) { return __builtin_clzll(x); }
#endif

using namespace exchange::core::collections::art;
using namespace exchange::core::collections::objpool;

// Default iterations for benchmarks
constexpr int kNumIterations = 3;

// Helper class to collect key-value pairs for int64_t
class TestConsumerInt64 : public LongObjConsumer<int64_t> {
public:
  std::vector<int64_t> keys;
  std::vector<int64_t *> values;

  void Accept(int64_t key, int64_t *value) override {
    keys.push_back(key);
    values.push_back(value);
  }

  void Clear() {
    keys.clear();
    values.clear();
  }
};

// Generate data using the same strategy as Java version
class DataGenerator {
public:
  std::mt19937 rng;
  std::uniform_int_distribution<int> stepDist;

  DataGenerator(int seed = 1) : rng(seed) {}

  // Step function: 1 + rand.nextInt((int) Math.min(Integer.MAX_VALUE, 1L +
  // (Long.highestOneBit(i) >> 8)))
  int StepFunction(int i) {
    int64_t highestBit =
        i == 0 ? 0 : (1LL << (63 - CountLeadingZeros64(static_cast<uint64_t>(i))));
    int maxStep = static_cast<int>(
        std::min(static_cast<int64_t>(INT_MAX),
                 static_cast<int64_t>(1LL + (highestBit >> 8))));
    std::uniform_int_distribution<int> dist(0, maxStep);
    return 1 + dist(rng);
  }

  std::vector<int64_t> GenerateData(int num, int64_t offset) {
    std::vector<int64_t> list;
    list.reserve(num);

    int64_t j = 0;
    for (int i = 0; i < num; i++) {
      list.push_back(offset + j);
      j += StepFunction(i);
    }

    // Shuffle
    std::shuffle(list.begin(), list.end(), rng);
    return list;
  }
};

// Benchmark fixture
class ArtTreeBenchmark : public benchmark::Fixture {
public:
  void SetUp(const ::benchmark::State &state) override {
    objectsPool_ = ObjectsPool::CreateDefaultTestPool();
    // 1. ART - single structure
    art_ = new LongAdaptiveRadixTreeMap<int64_t>(objectsPool_);
    // 2. std::map - single structure
    bst_ = new std::map<int64_t, int64_t>();
    // 3. unordered_map + set - hybrid for ordered ops
    unordered_map_ = new std::unordered_map<int64_t, int64_t>();
    unordered_set_ = new std::set<int64_t>();
    // 4. dense_map + set - hybrid for ordered ops
    dense_map_ = new ankerl::unordered_dense::map<int64_t, int64_t>();
    dense_set_ = new std::set<int64_t>();

    // Generate test data
    DataGenerator gen(1);
    int64_t offset = 1'000'000'000LL + gen.rng() % 1'000'000;
    data_ = gen.GenerateData(5'000'000, offset);

    // Pre-allocate values
    values_.reserve(data_.size());
    for (size_t i = 0; i < data_.size(); ++i) {
      values_.push_back(new int64_t(data_[i]));
    }
  }

  void TearDown(const ::benchmark::State &state) override {
    // Clean up values
    for (int64_t *v : values_) {
      delete v;
    }
    values_.clear();

    delete art_;
    delete bst_;
    delete unordered_map_;
    delete unordered_set_;
    delete dense_map_;
    delete dense_set_;
    delete objectsPool_;
  }

  ObjectsPool *objectsPool_;
  // 1. ART
  LongAdaptiveRadixTreeMap<int64_t> *art_;
  // 2. std::map (BST)
  std::map<int64_t, int64_t> *bst_;
  // 3. unordered_map + set (hybrid)
  std::unordered_map<int64_t, int64_t> *unordered_map_;
  std::set<int64_t> *unordered_set_;
  // 4. dense_map + set (hybrid)
  ankerl::unordered_dense::map<int64_t, int64_t> *dense_map_;
  std::set<int64_t> *dense_set_;

  std::vector<int64_t> data_;
  std::vector<int64_t *> values_;
};

// Helper to execute in random order (like Java version)
void ExecuteInRandomOrder(std::mt19937 &rng, std::function<void()> a,
                          std::function<void()> b) {
  if (rng() % 2 == 0) {
    a();
    b();
  } else {
    b();
    a();
  }
}

// Helper functions
static double NanoToMs(int64_t nano) {
  return static_cast<double>(nano) / 1000000.0;
}

static double PercentImprovement(int64_t oldTime, int64_t newTime) {
  if (newTime == 0)
    return 0.0;
  // Calculate: 100 * (oldTime / newTime - 1)
  // This gives the percentage improvement (positive = new is faster)
  // Example: oldTime=680M, newTime=123M -> 100 * (680/123 - 1) = 100 * 4.51 =
  // 451%
  double ratio = static_cast<double>(oldTime) / static_cast<double>(newTime);
  return 100.0 * (ratio - 1.0);
}

// Benchmark: PUT
// Compare 4 solutions that ALL support ordered operations (Higher/Lower):
// 1. ART - single structure
// 2. std::map - single structure
// 3. unordered_map + set - hybrid
// 4. dense_map + set - hybrid
BENCHMARK_DEFINE_F(ArtTreeBenchmark, Put)(benchmark::State &state) {
  std::mt19937 rng(1);

  for (auto _ : state) {
    // Clear all structures
    art_->Clear();
    bst_->clear();
    unordered_map_->clear();
    unordered_set_->clear();
    dense_map_->clear();
    dense_set_->clear();

    // Shuffle data
    std::vector<int64_t> shuffled = data_;
    std::shuffle(shuffled.begin(), shuffled.end(), rng);

    // 1. Measure ART
    auto start = std::chrono::high_resolution_clock::now();
    for (size_t i = 0; i < shuffled.size(); ++i) {
      art_->Put(shuffled[i], values_[i]);
    }
    auto end = std::chrono::high_resolution_clock::now();
    auto artTime =
        std::chrono::duration_cast<std::chrono::nanoseconds>(end - start)
            .count();

    // 2. Measure std::map (BST)
    start = std::chrono::high_resolution_clock::now();
    for (size_t i = 0; i < shuffled.size(); ++i) {
      (*bst_)[shuffled[i]] = shuffled[i];
    }
    end = std::chrono::high_resolution_clock::now();
    auto bstTime =
        std::chrono::duration_cast<std::chrono::nanoseconds>(end - start)
            .count();

    // 3. Measure unordered_map + set (hybrid)
    start = std::chrono::high_resolution_clock::now();
    for (size_t i = 0; i < shuffled.size(); ++i) {
      (*unordered_map_)[shuffled[i]] = shuffled[i];
      unordered_set_->insert(shuffled[i]);
    }
    end = std::chrono::high_resolution_clock::now();
    auto unorderedHybridTime =
        std::chrono::duration_cast<std::chrono::nanoseconds>(end - start)
            .count();

    // 4. Measure dense_map + set (hybrid)
    start = std::chrono::high_resolution_clock::now();
    for (size_t i = 0; i < shuffled.size(); ++i) {
      (*dense_map_)[shuffled[i]] = shuffled[i];
      dense_set_->insert(shuffled[i]);
    }
    end = std::chrono::high_resolution_clock::now();
    auto denseHybridTime =
        std::chrono::duration_cast<std::chrono::nanoseconds>(end - start)
            .count();

    // Time counters (ns)
    state.counters["1_art"] =
        benchmark::Counter(artTime, benchmark::Counter::kAvgIterations);
    state.counters["2_bst"] =
        benchmark::Counter(bstTime, benchmark::Counter::kAvgIterations);
    state.counters["3_uo_h"] =
        benchmark::Counter(unorderedHybridTime, benchmark::Counter::kAvgIterations);
    state.counters["4_de_h"] =
        benchmark::Counter(denseHybridTime, benchmark::Counter::kAvgIterations);
    // Improvement percentages (ART vs others)
    state.counters["vs_bst%"] = benchmark::Counter(
        PercentImprovement(bstTime, artTime) * kNumIterations,
        benchmark::Counter::kAvgIterations);
    state.counters["vs_uo%"] = benchmark::Counter(
        PercentImprovement(unorderedHybridTime, artTime) * kNumIterations,
        benchmark::Counter::kAvgIterations);
    state.counters["vs_de%"] = benchmark::Counter(
        PercentImprovement(denseHybridTime, artTime) * kNumIterations,
        benchmark::Counter::kAvgIterations);
  }
}
BENCHMARK_REGISTER_F(ArtTreeBenchmark, Put)->Iterations(kNumIterations);

// Benchmark: GET_HIT
// For Get operation, hybrid solutions only need to query the map (set is for ordering)
// But we still compare all 4 complete solutions
BENCHMARK_DEFINE_F(ArtTreeBenchmark, GetHit)(benchmark::State &state) {
  // Pre-populate all 4 solutions
  for (size_t i = 0; i < data_.size(); ++i) {
    art_->Put(data_[i], values_[i]);
    (*bst_)[data_[i]] = data_[i];
    (*unordered_map_)[data_[i]] = data_[i];
    unordered_set_->insert(data_[i]);
    (*dense_map_)[data_[i]] = data_[i];
    dense_set_->insert(data_[i]);
  }

  std::mt19937 rng(1);

  for (auto _ : state) {
    // Shuffle data
    std::vector<int64_t> shuffled = data_;
    std::shuffle(shuffled.begin(), shuffled.end(), rng);

    // 1. Measure ART
    int64_t artSum = 0;
    auto start = std::chrono::high_resolution_clock::now();
    for (int64_t key : shuffled) {
      int64_t *v = art_->Get(key);
      if (v)
        artSum += *v;
    }
    auto end = std::chrono::high_resolution_clock::now();
    auto artTime =
        std::chrono::duration_cast<std::chrono::nanoseconds>(end - start)
            .count();

    // 2. Measure std::map (BST)
    int64_t bstSum = 0;
    start = std::chrono::high_resolution_clock::now();
    for (int64_t key : shuffled) {
      auto it = bst_->find(key);
      if (it != bst_->end())
        bstSum += it->second;
    }
    end = std::chrono::high_resolution_clock::now();
    auto bstTime =
        std::chrono::duration_cast<std::chrono::nanoseconds>(end - start)
            .count();

    // 3. Measure unordered_map + set (hybrid) - Get only needs map
    int64_t unorderedSum = 0;
    start = std::chrono::high_resolution_clock::now();
    for (int64_t key : shuffled) {
      auto it = unordered_map_->find(key);
      if (it != unordered_map_->end())
        unorderedSum += it->second;
    }
    end = std::chrono::high_resolution_clock::now();
    auto unorderedHybridTime =
        std::chrono::duration_cast<std::chrono::nanoseconds>(end - start)
            .count();

    // 4. Measure dense_map + set (hybrid) - Get only needs map
    int64_t denseSum = 0;
    start = std::chrono::high_resolution_clock::now();
    for (int64_t key : shuffled) {
      auto it = dense_map_->find(key);
      if (it != dense_map_->end())
        denseSum += it->second;
    }
    end = std::chrono::high_resolution_clock::now();
    auto denseHybridTime =
        std::chrono::duration_cast<std::chrono::nanoseconds>(end - start)
            .count();

    // Time counters (ns)
    state.counters["1_art"] =
        benchmark::Counter(artTime, benchmark::Counter::kAvgIterations);
    state.counters["2_bst"] =
        benchmark::Counter(bstTime, benchmark::Counter::kAvgIterations);
    state.counters["3_uo_h"] =
        benchmark::Counter(unorderedHybridTime, benchmark::Counter::kAvgIterations);
    state.counters["4_de_h"] =
        benchmark::Counter(denseHybridTime, benchmark::Counter::kAvgIterations);
    // Improvement percentages
    state.counters["vs_bst%"] = benchmark::Counter(
        PercentImprovement(bstTime, artTime) * kNumIterations,
        benchmark::Counter::kAvgIterations);
    state.counters["vs_uo%"] = benchmark::Counter(
        PercentImprovement(unorderedHybridTime, artTime) * kNumIterations,
        benchmark::Counter::kAvgIterations);
    state.counters["vs_de%"] = benchmark::Counter(
        PercentImprovement(denseHybridTime, artTime) * kNumIterations,
        benchmark::Counter::kAvgIterations);
    // Correctness check (prevent optimization)
    state.counters["sum"] =
        benchmark::Counter(artSum + bstSum + unorderedSum + denseSum,
                           benchmark::Counter::kAvgIterations);
  }
}
BENCHMARK_REGISTER_F(ArtTreeBenchmark, GetHit)->Iterations(kNumIterations);

// Benchmark: GET_MISS (query non-existent keys)
// This tests the performance of failed lookups - ART can terminate early
BENCHMARK_DEFINE_F(ArtTreeBenchmark, GetMiss)(benchmark::State &state) {
  // Pre-populate all 4 solutions
  for (size_t i = 0; i < data_.size(); ++i) {
    art_->Put(data_[i], values_[i]);
    (*bst_)[data_[i]] = data_[i];
    (*unordered_map_)[data_[i]] = data_[i];
    unordered_set_->insert(data_[i]);
    (*dense_map_)[data_[i]] = data_[i];
    dense_set_->insert(data_[i]);
  }

  // Generate non-existent keys by adding a large offset to existing keys
  std::vector<int64_t> missKeys;
  missKeys.reserve(data_.size());
  constexpr int64_t kMissOffset = 1'000'000'000'000LL; // 1 trillion offset
  for (int64_t key : data_) {
    missKeys.push_back(key + kMissOffset);
  }

  std::mt19937 rng(1);

  for (auto _ : state) {
    // Shuffle miss keys
    std::vector<int64_t> shuffled = missKeys;
    std::shuffle(shuffled.begin(), shuffled.end(), rng);

    // 1. Measure ART
    int64_t artMissCount = 0;
    auto start = std::chrono::high_resolution_clock::now();
    for (int64_t key : shuffled) {
      int64_t *v = art_->Get(key);
      if (!v)
        artMissCount++;
    }
    auto end = std::chrono::high_resolution_clock::now();
    auto artTime =
        std::chrono::duration_cast<std::chrono::nanoseconds>(end - start)
            .count();

    // 2. Measure std::map (BST)
    int64_t bstMissCount = 0;
    start = std::chrono::high_resolution_clock::now();
    for (int64_t key : shuffled) {
      auto it = bst_->find(key);
      if (it == bst_->end())
        bstMissCount++;
    }
    end = std::chrono::high_resolution_clock::now();
    auto bstTime =
        std::chrono::duration_cast<std::chrono::nanoseconds>(end - start)
            .count();

    // 3. Measure unordered_map + set (hybrid) - Get only needs map
    int64_t unorderedMissCount = 0;
    start = std::chrono::high_resolution_clock::now();
    for (int64_t key : shuffled) {
      auto it = unordered_map_->find(key);
      if (it == unordered_map_->end())
        unorderedMissCount++;
    }
    end = std::chrono::high_resolution_clock::now();
    auto unorderedHybridTime =
        std::chrono::duration_cast<std::chrono::nanoseconds>(end - start)
            .count();

    // 4. Measure dense_map + set (hybrid) - Get only needs map
    int64_t denseMissCount = 0;
    start = std::chrono::high_resolution_clock::now();
    for (int64_t key : shuffled) {
      auto it = dense_map_->find(key);
      if (it == dense_map_->end())
        denseMissCount++;
    }
    end = std::chrono::high_resolution_clock::now();
    auto denseHybridTime =
        std::chrono::duration_cast<std::chrono::nanoseconds>(end - start)
            .count();

    // Time counters (ns)
    state.counters["1_art"] =
        benchmark::Counter(artTime, benchmark::Counter::kAvgIterations);
    state.counters["2_bst"] =
        benchmark::Counter(bstTime, benchmark::Counter::kAvgIterations);
    state.counters["3_uo_h"] =
        benchmark::Counter(unorderedHybridTime, benchmark::Counter::kAvgIterations);
    state.counters["4_de_h"] =
        benchmark::Counter(denseHybridTime, benchmark::Counter::kAvgIterations);
    // Improvement percentages
    state.counters["vs_bst%"] = benchmark::Counter(
        PercentImprovement(bstTime, artTime) * kNumIterations,
        benchmark::Counter::kAvgIterations);
    state.counters["vs_uo%"] = benchmark::Counter(
        PercentImprovement(unorderedHybridTime, artTime) * kNumIterations,
        benchmark::Counter::kAvgIterations);
    state.counters["vs_de%"] = benchmark::Counter(
        PercentImprovement(denseHybridTime, artTime) * kNumIterations,
        benchmark::Counter::kAvgIterations);
    // Correctness check
    state.counters["miss"] = benchmark::Counter(
        artMissCount + bstMissCount + denseMissCount + unorderedMissCount,
        benchmark::Counter::kAvgIterations);
  }
}
BENCHMARK_REGISTER_F(ArtTreeBenchmark, GetMiss)->Iterations(kNumIterations);

// Benchmark: REMOVE
// Hybrid solutions must erase from both map and set
BENCHMARK_DEFINE_F(ArtTreeBenchmark, Remove)(benchmark::State &state) {
  std::mt19937 rng(1);

  for (auto _ : state) {
    // Pre-populate all 4 solutions
    art_->Clear();
    bst_->clear();
    unordered_map_->clear();
    unordered_set_->clear();
    dense_map_->clear();
    dense_set_->clear();
    for (size_t i = 0; i < data_.size(); ++i) {
      art_->Put(data_[i], values_[i]);
      (*bst_)[data_[i]] = data_[i];
      (*unordered_map_)[data_[i]] = data_[i];
      unordered_set_->insert(data_[i]);
      (*dense_map_)[data_[i]] = data_[i];
      dense_set_->insert(data_[i]);
    }

    // Shuffle data
    std::vector<int64_t> shuffled = data_;
    std::shuffle(shuffled.begin(), shuffled.end(), rng);

    // 1. Measure ART
    auto start = std::chrono::high_resolution_clock::now();
    for (int64_t key : shuffled) {
      art_->Remove(key);
    }
    auto end = std::chrono::high_resolution_clock::now();
    auto artTime =
        std::chrono::duration_cast<std::chrono::nanoseconds>(end - start)
            .count();

    // 2. Measure std::map (BST)
    start = std::chrono::high_resolution_clock::now();
    for (int64_t key : shuffled) {
      bst_->erase(key);
    }
    end = std::chrono::high_resolution_clock::now();
    auto bstTime =
        std::chrono::duration_cast<std::chrono::nanoseconds>(end - start)
            .count();

    // 3. Measure unordered_map + set (hybrid) - must erase from both
    start = std::chrono::high_resolution_clock::now();
    for (int64_t key : shuffled) {
      unordered_map_->erase(key);
      unordered_set_->erase(key);
    }
    end = std::chrono::high_resolution_clock::now();
    auto unorderedHybridTime =
        std::chrono::duration_cast<std::chrono::nanoseconds>(end - start)
            .count();

    // 4. Measure dense_map + set (hybrid) - must erase from both
    start = std::chrono::high_resolution_clock::now();
    for (int64_t key : shuffled) {
      dense_map_->erase(key);
      dense_set_->erase(key);
    }
    end = std::chrono::high_resolution_clock::now();
    auto denseHybridTime =
        std::chrono::duration_cast<std::chrono::nanoseconds>(end - start)
            .count();

    // Time counters (ns)
    state.counters["1_art"] =
        benchmark::Counter(artTime, benchmark::Counter::kAvgIterations);
    state.counters["2_bst"] =
        benchmark::Counter(bstTime, benchmark::Counter::kAvgIterations);
    state.counters["3_uo_h"] =
        benchmark::Counter(unorderedHybridTime, benchmark::Counter::kAvgIterations);
    state.counters["4_de_h"] =
        benchmark::Counter(denseHybridTime, benchmark::Counter::kAvgIterations);
    // Improvement percentages
    state.counters["vs_bst%"] = benchmark::Counter(
        PercentImprovement(bstTime, artTime) * kNumIterations,
        benchmark::Counter::kAvgIterations);
    state.counters["vs_uo%"] = benchmark::Counter(
        PercentImprovement(unorderedHybridTime, artTime) * kNumIterations,
        benchmark::Counter::kAvgIterations);
    state.counters["vs_de%"] = benchmark::Counter(
        PercentImprovement(denseHybridTime, artTime) * kNumIterations,
        benchmark::Counter::kAvgIterations);
  }
}
BENCHMARK_REGISTER_F(ArtTreeBenchmark, Remove)->Iterations(kNumIterations);

// Benchmark: FOREACH
BENCHMARK_DEFINE_F(ArtTreeBenchmark, ForEach)(benchmark::State &state) {
  constexpr int forEachSize = 5000;

  // Pre-populate
  for (size_t i = 0; i < data_.size(); ++i) {
    art_->Put(data_[i], values_[i]);
    (*bst_)[data_[i]] = data_[i];
    (*dense_map_)[data_[i]] = data_[i];
    (*unordered_map_)[data_[i]] = data_[i];
  }

  for (auto _ : state) {
    // Measure ART
    TestConsumerInt64 artConsumer;
    auto start = std::chrono::high_resolution_clock::now();
    art_->ForEach(&artConsumer, forEachSize);
    auto end = std::chrono::high_resolution_clock::now();
    auto artTime =
        std::chrono::duration_cast<std::chrono::nanoseconds>(end - start)
            .count();

    // Measure BST
    std::vector<int64_t> bstKeys;
    std::vector<int64_t> bstValues;
    bstKeys.reserve(forEachSize);
    bstValues.reserve(forEachSize);
    start = std::chrono::high_resolution_clock::now();
    int count = 0;
    for (const auto &pair : *bst_) {
      if (count >= forEachSize)
        break;
      bstKeys.push_back(pair.first);
      bstValues.push_back(pair.second);
      count++;
    }
    end = std::chrono::high_resolution_clock::now();
    auto bstTime =
        std::chrono::duration_cast<std::chrono::nanoseconds>(end - start)
            .count();

    // Measure ankerl::unordered_dense (unordered iteration)
    std::vector<int64_t> denseKeys;
    std::vector<int64_t> denseValues;
    denseKeys.reserve(forEachSize);
    denseValues.reserve(forEachSize);
    start = std::chrono::high_resolution_clock::now();
    count = 0;
    for (const auto &pair : *dense_map_) {
      if (count >= forEachSize)
        break;
      denseKeys.push_back(pair.first);
      denseValues.push_back(pair.second);
      count++;
    }
    end = std::chrono::high_resolution_clock::now();
    auto denseTime =
        std::chrono::duration_cast<std::chrono::nanoseconds>(end - start)
            .count();

    // Measure std::unordered_map (unordered iteration)
    std::vector<int64_t> unorderedKeys;
    std::vector<int64_t> unorderedValues;
    unorderedKeys.reserve(forEachSize);
    unorderedValues.reserve(forEachSize);
    start = std::chrono::high_resolution_clock::now();
    count = 0;
    for (const auto &pair : *unordered_map_) {
      if (count >= forEachSize)
        break;
      unorderedKeys.push_back(pair.first);
      unorderedValues.push_back(pair.second);
      count++;
    }
    end = std::chrono::high_resolution_clock::now();
    auto unorderedTime =
        std::chrono::duration_cast<std::chrono::nanoseconds>(end - start)
            .count();

    // Time counters (ns)
    state.counters["1_art"] =
        benchmark::Counter(artTime, benchmark::Counter::kAvgIterations);
    state.counters["2_bst"] =
        benchmark::Counter(bstTime, benchmark::Counter::kAvgIterations);
    state.counters["3_uo"] =
        benchmark::Counter(unorderedTime, benchmark::Counter::kAvgIterations);
    state.counters["4_de"] =
        benchmark::Counter(denseTime, benchmark::Counter::kAvgIterations);
    // Improvement percentages
    state.counters["vs_bst%"] = benchmark::Counter(
        PercentImprovement(bstTime, artTime) * kNumIterations,
        benchmark::Counter::kAvgIterations);
    state.counters["vs_uo%"] = benchmark::Counter(
        PercentImprovement(unorderedTime, artTime) * kNumIterations,
        benchmark::Counter::kAvgIterations);
    state.counters["vs_de%"] = benchmark::Counter(
        PercentImprovement(denseTime, artTime) * kNumIterations,
        benchmark::Counter::kAvgIterations);
    // Correctness check
    state.counters["cnt"] = benchmark::Counter(
        artConsumer.keys.size() + bstKeys.size() + denseKeys.size() +
            unorderedKeys.size(),
        benchmark::Counter::kAvgIterations);
  }
}
BENCHMARK_REGISTER_F(ArtTreeBenchmark, ForEach)->Iterations(kNumIterations);

// Benchmark: FOREACH_DESC
BENCHMARK_DEFINE_F(ArtTreeBenchmark, ForEachDesc)(benchmark::State &state) {
  constexpr int forEachSize = 5000;

  // Pre-populate
  for (size_t i = 0; i < data_.size(); ++i) {
    art_->Put(data_[i], values_[i]);
    (*bst_)[data_[i]] = data_[i];
    (*dense_map_)[data_[i]] = data_[i];
    (*unordered_map_)[data_[i]] = data_[i];
  }

  for (auto _ : state) {
    // Measure ART
    TestConsumerInt64 artConsumer;
    auto start = std::chrono::high_resolution_clock::now();
    art_->ForEachDesc(&artConsumer, forEachSize);
    auto end = std::chrono::high_resolution_clock::now();
    auto artTime =
        std::chrono::duration_cast<std::chrono::nanoseconds>(end - start)
            .count();

    // Measure BST (descending)
    std::vector<int64_t> bstKeys;
    std::vector<int64_t> bstValues;
    bstKeys.reserve(forEachSize);
    bstValues.reserve(forEachSize);
    start = std::chrono::high_resolution_clock::now();
    int count = 0;
    for (auto it = bst_->rbegin(); it != bst_->rend() && count < forEachSize;
         ++it) {
      bstKeys.push_back(it->first);
      bstValues.push_back(it->second);
      count++;
    }
    end = std::chrono::high_resolution_clock::now();
    auto bstTime =
        std::chrono::duration_cast<std::chrono::nanoseconds>(end - start)
            .count();

    // Note: Hash maps don't support ordered descending iteration
    // We'll still measure unordered iteration for comparison
    std::vector<int64_t> denseKeys;
    std::vector<int64_t> denseValues;
    denseKeys.reserve(forEachSize);
    denseValues.reserve(forEachSize);
    start = std::chrono::high_resolution_clock::now();
    count = 0;
    for (const auto &pair : *dense_map_) {
      if (count >= forEachSize)
        break;
      denseKeys.push_back(pair.first);
      denseValues.push_back(pair.second);
      count++;
    }
    end = std::chrono::high_resolution_clock::now();
    auto denseTime =
        std::chrono::duration_cast<std::chrono::nanoseconds>(end - start)
            .count();

    std::vector<int64_t> unorderedKeys;
    std::vector<int64_t> unorderedValues;
    unorderedKeys.reserve(forEachSize);
    unorderedValues.reserve(forEachSize);
    start = std::chrono::high_resolution_clock::now();
    count = 0;
    for (const auto &pair : *unordered_map_) {
      if (count >= forEachSize)
        break;
      unorderedKeys.push_back(pair.first);
      unorderedValues.push_back(pair.second);
      count++;
    }
    end = std::chrono::high_resolution_clock::now();
    auto unorderedTime =
        std::chrono::duration_cast<std::chrono::nanoseconds>(end - start)
            .count();

    // Time counters (ns)
    state.counters["1_art"] =
        benchmark::Counter(artTime, benchmark::Counter::kAvgIterations);
    state.counters["2_bst"] =
        benchmark::Counter(bstTime, benchmark::Counter::kAvgIterations);
    state.counters["3_uo"] =
        benchmark::Counter(unorderedTime, benchmark::Counter::kAvgIterations);
    state.counters["4_de"] =
        benchmark::Counter(denseTime, benchmark::Counter::kAvgIterations);
    // Improvement percentages
    state.counters["vs_bst%"] = benchmark::Counter(
        PercentImprovement(bstTime, artTime) * kNumIterations,
        benchmark::Counter::kAvgIterations);
    state.counters["vs_uo%"] = benchmark::Counter(
        PercentImprovement(unorderedTime, artTime) * kNumIterations,
        benchmark::Counter::kAvgIterations);
    state.counters["vs_de%"] = benchmark::Counter(
        PercentImprovement(denseTime, artTime) * kNumIterations,
        benchmark::Counter::kAvgIterations);
  }
}
BENCHMARK_REGISTER_F(ArtTreeBenchmark, ForEachDesc)->Iterations(kNumIterations);

// Benchmark: HIGHER (upper_bound - find first key strictly greater than given)
// Compare all 4 solutions that support ordered operations:
// Hybrid solutions use set.upper_bound to find key, then map to get value
BENCHMARK_DEFINE_F(ArtTreeBenchmark, Higher)(benchmark::State &state) {
  // Pre-populate all 4 solutions
  for (size_t i = 0; i < data_.size(); ++i) {
    art_->Put(data_[i], values_[i]);
    (*bst_)[data_[i]] = data_[i];
    (*unordered_map_)[data_[i]] = data_[i];
    unordered_set_->insert(data_[i]);
    (*dense_map_)[data_[i]] = data_[i];
    dense_set_->insert(data_[i]);
  }

  std::mt19937 rng(1);

  for (auto _ : state) {
    // Shuffle data
    std::vector<int64_t> shuffled = data_;
    std::shuffle(shuffled.begin(), shuffled.end(), rng);

    // 1. Measure ART
    int64_t artSum = 0;
    auto start = std::chrono::high_resolution_clock::now();
    for (int64_t key : shuffled) {
      int64_t *v = art_->GetHigherValue(key);
      if (v)
        artSum += *v;
    }
    auto end = std::chrono::high_resolution_clock::now();
    auto artTime =
        std::chrono::duration_cast<std::chrono::nanoseconds>(end - start)
            .count();

    // 2. Measure std::map (BST)
    int64_t bstSum = 0;
    start = std::chrono::high_resolution_clock::now();
    for (int64_t key : shuffled) {
      auto it = bst_->upper_bound(key);
      if (it != bst_->end())
        bstSum += it->second;
    }
    end = std::chrono::high_resolution_clock::now();
    auto bstTime =
        std::chrono::duration_cast<std::chrono::nanoseconds>(end - start)
            .count();

    // 3. Measure unordered_map + set (hybrid): set.upper_bound + map lookup
    int64_t unorderedSum = 0;
    start = std::chrono::high_resolution_clock::now();
    for (int64_t key : shuffled) {
      auto setIt = unordered_set_->upper_bound(key);
      if (setIt != unordered_set_->end()) {
        auto mapIt = unordered_map_->find(*setIt);
        if (mapIt != unordered_map_->end())
          unorderedSum += mapIt->second;
      }
    }
    end = std::chrono::high_resolution_clock::now();
    auto unorderedHybridTime =
        std::chrono::duration_cast<std::chrono::nanoseconds>(end - start)
            .count();

    // 4. Measure dense_map + set (hybrid): set.upper_bound + map lookup
    int64_t denseSum = 0;
    start = std::chrono::high_resolution_clock::now();
    for (int64_t key : shuffled) {
      auto setIt = dense_set_->upper_bound(key);
      if (setIt != dense_set_->end()) {
        auto mapIt = dense_map_->find(*setIt);
        if (mapIt != dense_map_->end())
          denseSum += mapIt->second;
      }
    }
    end = std::chrono::high_resolution_clock::now();
    auto denseHybridTime =
        std::chrono::duration_cast<std::chrono::nanoseconds>(end - start)
            .count();

    // Time counters (ns)
    state.counters["1_art"] =
        benchmark::Counter(artTime, benchmark::Counter::kAvgIterations);
    state.counters["2_bst"] =
        benchmark::Counter(bstTime, benchmark::Counter::kAvgIterations);
    state.counters["3_uo_h"] =
        benchmark::Counter(unorderedHybridTime, benchmark::Counter::kAvgIterations);
    state.counters["4_de_h"] =
        benchmark::Counter(denseHybridTime, benchmark::Counter::kAvgIterations);
    // Improvement percentages
    state.counters["vs_bst%"] = benchmark::Counter(
        PercentImprovement(bstTime, artTime) * kNumIterations,
        benchmark::Counter::kAvgIterations);
    state.counters["vs_uo%"] = benchmark::Counter(
        PercentImprovement(unorderedHybridTime, artTime) * kNumIterations,
        benchmark::Counter::kAvgIterations);
    state.counters["vs_de%"] = benchmark::Counter(
        PercentImprovement(denseHybridTime, artTime) * kNumIterations,
        benchmark::Counter::kAvgIterations);
    // Correctness check (prevent optimization)
    state.counters["sum"] =
        benchmark::Counter(artSum + bstSum + unorderedSum + denseSum,
                           benchmark::Counter::kAvgIterations);
  }
}
BENCHMARK_REGISTER_F(ArtTreeBenchmark, Higher)->Iterations(kNumIterations);

// Benchmark: LOWER (find first key strictly less than given)
// Compare all 4 solutions that support ordered operations:
// Hybrid solutions use set.lower_bound then decrement, then map to get value
BENCHMARK_DEFINE_F(ArtTreeBenchmark, Lower)(benchmark::State &state) {
  // Pre-populate all 4 solutions
  for (size_t i = 0; i < data_.size(); ++i) {
    art_->Put(data_[i], values_[i]);
    (*bst_)[data_[i]] = data_[i];
    (*unordered_map_)[data_[i]] = data_[i];
    unordered_set_->insert(data_[i]);
    (*dense_map_)[data_[i]] = data_[i];
    dense_set_->insert(data_[i]);
  }

  std::mt19937 rng(1);

  for (auto _ : state) {
    // Shuffle data
    std::vector<int64_t> shuffled = data_;
    std::shuffle(shuffled.begin(), shuffled.end(), rng);

    // 1. Measure ART
    int64_t artSum = 0;
    auto start = std::chrono::high_resolution_clock::now();
    for (int64_t key : shuffled) {
      int64_t *v = art_->GetLowerValue(key);
      if (v)
        artSum += *v;
    }
    auto end = std::chrono::high_resolution_clock::now();
    auto artTime =
        std::chrono::duration_cast<std::chrono::nanoseconds>(end - start)
            .count();

    // 2. Measure std::map (BST)
    int64_t bstSum = 0;
    start = std::chrono::high_resolution_clock::now();
    for (int64_t key : shuffled) {
      auto it = bst_->lower_bound(key);
      if (it != bst_->begin()) {
        --it;
        bstSum += it->second;
      }
    }
    end = std::chrono::high_resolution_clock::now();
    auto bstTime =
        std::chrono::duration_cast<std::chrono::nanoseconds>(end - start)
            .count();

    // 3. Measure unordered_map + set (hybrid): set.lower_bound - 1 + map lookup
    int64_t unorderedSum = 0;
    start = std::chrono::high_resolution_clock::now();
    for (int64_t key : shuffled) {
      auto setIt = unordered_set_->lower_bound(key);
      if (setIt != unordered_set_->begin()) {
        --setIt;
        auto mapIt = unordered_map_->find(*setIt);
        if (mapIt != unordered_map_->end())
          unorderedSum += mapIt->second;
      }
    }
    end = std::chrono::high_resolution_clock::now();
    auto unorderedHybridTime =
        std::chrono::duration_cast<std::chrono::nanoseconds>(end - start)
            .count();

    // 4. Measure dense_map + set (hybrid): set.lower_bound - 1 + map lookup
    int64_t denseSum = 0;
    start = std::chrono::high_resolution_clock::now();
    for (int64_t key : shuffled) {
      auto setIt = dense_set_->lower_bound(key);
      if (setIt != dense_set_->begin()) {
        --setIt;
        auto mapIt = dense_map_->find(*setIt);
        if (mapIt != dense_map_->end())
          denseSum += mapIt->second;
      }
    }
    end = std::chrono::high_resolution_clock::now();
    auto denseHybridTime =
        std::chrono::duration_cast<std::chrono::nanoseconds>(end - start)
            .count();

    // Time counters (ns)
    state.counters["1_art"] =
        benchmark::Counter(artTime, benchmark::Counter::kAvgIterations);
    state.counters["2_bst"] =
        benchmark::Counter(bstTime, benchmark::Counter::kAvgIterations);
    state.counters["3_uo_h"] =
        benchmark::Counter(unorderedHybridTime, benchmark::Counter::kAvgIterations);
    state.counters["4_de_h"] =
        benchmark::Counter(denseHybridTime, benchmark::Counter::kAvgIterations);
    // Improvement percentages
    state.counters["vs_bst%"] = benchmark::Counter(
        PercentImprovement(bstTime, artTime) * kNumIterations,
        benchmark::Counter::kAvgIterations);
    state.counters["vs_uo%"] = benchmark::Counter(
        PercentImprovement(unorderedHybridTime, artTime) * kNumIterations,
        benchmark::Counter::kAvgIterations);
    state.counters["vs_de%"] = benchmark::Counter(
        PercentImprovement(denseHybridTime, artTime) * kNumIterations,
        benchmark::Counter::kAvgIterations);
    // Correctness check (prevent optimization)
    state.counters["sum"] =
        benchmark::Counter(artSum + bstSum + unorderedSum + denseSum,
                           benchmark::Counter::kAvgIterations);
  }
}
BENCHMARK_REGISTER_F(ArtTreeBenchmark, Lower)->Iterations(kNumIterations);

BENCHMARK_MAIN();
