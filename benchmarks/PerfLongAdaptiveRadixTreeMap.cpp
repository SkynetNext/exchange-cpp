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

#include <ankerl/unordered_dense.h>
#include <benchmark/benchmark.h>
#include <exchange/core/collections/art/LongAdaptiveRadixTreeMap.h>
#include <exchange/core/collections/art/LongObjConsumer.h>
#include <exchange/core/collections/objpool/ObjectsPool.h>
#include <algorithm>
#include <chrono>
#include <cstdint>
#include <map>
// On Windows, mimalloc-static doesn't automatically override C++ new/delete,
// so we need to include mimalloc-new-delete.h explicitly.
// On Linux/Unix, mimalloc-static automatically overrides via linking.
#ifdef _WIN32
#  include <mimalloc-new-delete.h>
#endif
#include <random>
#include <unordered_map>
#include <vector>

// Cross-platform leading zero count
#ifdef _MSC_VER
#  include <intrin.h>

inline int CountLeadingZeros64(uint64_t x) {
  unsigned long index;
  return _BitScanReverse64(&index, x) ? (63 - static_cast<int>(index)) : 64;
}
#else
inline int CountLeadingZeros64(uint64_t x) {
  return __builtin_clzll(x);
}
#endif

using namespace exchange::core::collections::art;
using namespace exchange::core::collections::objpool;

// Default iterations for benchmarks
constexpr int kNumIterations = 3;

// Helper class to collect key-value pairs for int64_t
class TestConsumerInt64 : public LongObjConsumer<int64_t> {
public:
  std::vector<int64_t> keys;
  std::vector<int64_t*> values;

  void Accept(int64_t key, int64_t* value) override {
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

  explicit DataGenerator(int seed = 1) : rng(seed) {}

  // Step function: 1 + rand.nextInt((int) Math.min(Integer.MAX_VALUE, 1L +
  // (Long.highestOneBit(i) >> 8)))
  int StepFunction(int i) {
    int64_t highestBit = i == 0 ? 0 : (1LL << (63 - CountLeadingZeros64(static_cast<uint64_t>(i))));
    int maxStep = static_cast<int>(
      std::min(static_cast<int64_t>(INT_MAX), static_cast<int64_t>(1LL + (highestBit >> 8))));
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

// Helper functions (must be before class that uses them)
static double PercentImprovement(int64_t oldTime, int64_t newTime) {
  if (newTime == 0)
    return 0.0;
  double ratio = static_cast<double>(oldTime) / static_cast<double>(newTime);
  return 100.0 * (ratio - 1.0);
}

// Benchmark fixture - testing pure single data structures
class ArtTreeBenchmark : public benchmark::Fixture {
public:
  void SetUp(const ::benchmark::State& state) override {
    objectsPool_ = ObjectsPool::CreateDefaultTestPool();
    // 1. ART
    art_ = new LongAdaptiveRadixTreeMap<int64_t>(objectsPool_);
    // 2. std::map (BST/Red-Black Tree)
    bst_ = new std::map<int64_t, int64_t>();
    // 3. std::unordered_map
    unordered_map_ = new std::unordered_map<int64_t, int64_t>();
    // 4. ankerl::unordered_dense::map
    dense_map_ = new ankerl::unordered_dense::map<int64_t, int64_t>();
    // 5. std::set (for ordered operations comparison)
    set_ = new std::set<int64_t>();

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

  void TearDown(const ::benchmark::State& state) override {
    // Clean up values
    for (int64_t* v : values_) {
      delete v;
    }
    values_.clear();

    delete art_;
    delete bst_;
    delete unordered_map_;
    delete dense_map_;
    delete set_;
    delete objectsPool_;
  }

  // Helper to report timing counters
  void ReportCounters(benchmark::State& state,
                      int64_t artTime,
                      int64_t bstTime,
                      int64_t uoTime,
                      int64_t deTime,
                      int64_t setTime = -1) {
    state.counters["1_art"] = benchmark::Counter(artTime, benchmark::Counter::kAvgIterations);
    state.counters["2_bst"] = benchmark::Counter(bstTime, benchmark::Counter::kAvgIterations);
    state.counters["3_uo"] = benchmark::Counter(uoTime, benchmark::Counter::kAvgIterations);
    state.counters["4_de"] = benchmark::Counter(deTime, benchmark::Counter::kAvgIterations);
    if (setTime >= 0) {
      state.counters["5_set"] = benchmark::Counter(setTime, benchmark::Counter::kAvgIterations);
    }
    // Improvement percentages (ART vs others)
    state.counters["vs_bst%"] = benchmark::Counter(
      PercentImprovement(bstTime, artTime) * kNumIterations, benchmark::Counter::kAvgIterations);
    state.counters["vs_uo%"] = benchmark::Counter(
      PercentImprovement(uoTime, artTime) * kNumIterations, benchmark::Counter::kAvgIterations);
    state.counters["vs_de%"] = benchmark::Counter(
      PercentImprovement(deTime, artTime) * kNumIterations, benchmark::Counter::kAvgIterations);
    if (setTime >= 0) {
      state.counters["vs_set%"] = benchmark::Counter(
        PercentImprovement(setTime, artTime) * kNumIterations, benchmark::Counter::kAvgIterations);
    }
  }

  ObjectsPool* objectsPool_{};
  // 1. ART
  LongAdaptiveRadixTreeMap<int64_t>* art_{};
  // 2. std::map (BST)
  std::map<int64_t, int64_t>* bst_{};
  // 3. std::unordered_map
  std::unordered_map<int64_t, int64_t>* unordered_map_{};
  // 4. ankerl::unordered_dense::map
  ankerl::unordered_dense::map<int64_t, int64_t>* dense_map_{};
  // 5. std::set (for ordered ops)
  std::set<int64_t>* set_{};

  std::vector<int64_t> data_;
  std::vector<int64_t*> values_;
};

// Helper to execute in random order (like Java version)
void ExecuteInRandomOrder(std::mt19937& rng, std::function<void()> a, std::function<void()> b) {
  if (rng() % 2 == 0) {
    a();
    b();
  } else {
    b();
    a();
  }
}

// Benchmark: PUT - Compare pure single data structures
BENCHMARK_DEFINE_F(ArtTreeBenchmark, Put)(benchmark::State& state) {
  std::mt19937 rng(1);

  for (auto _ : state) {
    // Clear all structures
    art_->Clear();
    bst_->clear();
    unordered_map_->clear();
    dense_map_->clear();
    set_->clear();

    // Shuffle data
    std::vector<int64_t> shuffled = data_;
    std::shuffle(shuffled.begin(), shuffled.end(), rng);

    // 1. Measure ART
    auto start = std::chrono::high_resolution_clock::now();
    for (size_t i = 0; i < shuffled.size(); ++i) {
      art_->Put(shuffled[i], values_[i]);
    }
    auto end = std::chrono::high_resolution_clock::now();
    auto artTime = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();

    // 2. Measure std::map (BST)
    start = std::chrono::high_resolution_clock::now();
    for (size_t i = 0; i < shuffled.size(); ++i) {
      (*bst_)[shuffled[i]] = shuffled[i];
    }
    end = std::chrono::high_resolution_clock::now();
    auto bstTime = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();

    // 3. Measure std::unordered_map
    start = std::chrono::high_resolution_clock::now();
    for (size_t i = 0; i < shuffled.size(); ++i) {
      (*unordered_map_)[shuffled[i]] = shuffled[i];
    }
    end = std::chrono::high_resolution_clock::now();
    auto uoTime = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();

    // 4. Measure ankerl::unordered_dense
    start = std::chrono::high_resolution_clock::now();
    for (size_t i = 0; i < shuffled.size(); ++i) {
      (*dense_map_)[shuffled[i]] = shuffled[i];
    }
    end = std::chrono::high_resolution_clock::now();
    auto deTime = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();

    // 5. Measure std::set (insert key only, no value)
    start = std::chrono::high_resolution_clock::now();
    for (size_t i = 0; i < shuffled.size(); ++i) {
      set_->insert(shuffled[i]);
    }
    end = std::chrono::high_resolution_clock::now();
    auto setTime = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();

    ReportCounters(state, artTime, bstTime, uoTime, deTime, setTime);
  }
}

BENCHMARK_REGISTER_F(ArtTreeBenchmark, Put)->Iterations(kNumIterations);

// Benchmark: GET_HIT - Point lookup for existing keys
// Note: std::set doesn't store values, so not included
BENCHMARK_DEFINE_F(ArtTreeBenchmark, GetHit)(benchmark::State& state) {
  // Pre-populate all structures
  for (size_t i = 0; i < data_.size(); ++i) {
    art_->Put(data_[i], values_[i]);
    (*bst_)[data_[i]] = data_[i];
    (*unordered_map_)[data_[i]] = data_[i];
    (*dense_map_)[data_[i]] = data_[i];
  }

  std::mt19937 rng(1);

  for (auto _ : state) {
    std::vector<int64_t> shuffled = data_;
    std::shuffle(shuffled.begin(), shuffled.end(), rng);

    // 1. ART
    int64_t artSum = 0;
    auto start = std::chrono::high_resolution_clock::now();
    for (int64_t key : shuffled) {
      int64_t* v = art_->Get(key);
      if (v)
        artSum += *v;
    }
    auto end = std::chrono::high_resolution_clock::now();
    auto artTime = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();

    // 2. std::map
    int64_t bstSum = 0;
    start = std::chrono::high_resolution_clock::now();
    for (int64_t key : shuffled) {
      auto it = bst_->find(key);
      if (it != bst_->end())
        bstSum += it->second;
    }
    end = std::chrono::high_resolution_clock::now();
    auto bstTime = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();

    // 3. std::unordered_map
    int64_t uoSum = 0;
    start = std::chrono::high_resolution_clock::now();
    for (int64_t key : shuffled) {
      auto it = unordered_map_->find(key);
      if (it != unordered_map_->end())
        uoSum += it->second;
    }
    end = std::chrono::high_resolution_clock::now();
    auto uoTime = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();

    // 4. ankerl::unordered_dense
    int64_t deSum = 0;
    start = std::chrono::high_resolution_clock::now();
    for (int64_t key : shuffled) {
      auto it = dense_map_->find(key);
      if (it != dense_map_->end())
        deSum += it->second;
    }
    end = std::chrono::high_resolution_clock::now();
    auto deTime = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();

    ReportCounters(state, artTime, bstTime, uoTime, deTime);
    state.counters["sum"] =
      benchmark::Counter(artSum + bstSum + uoSum + deSum, benchmark::Counter::kAvgIterations);
  }
}

BENCHMARK_REGISTER_F(ArtTreeBenchmark, GetHit)->Iterations(kNumIterations);

// Benchmark: GET_MISS - Query non-existent keys (ART can terminate early)
BENCHMARK_DEFINE_F(ArtTreeBenchmark, GetMiss)(benchmark::State& state) {
  // Pre-populate all structures
  for (size_t i = 0; i < data_.size(); ++i) {
    art_->Put(data_[i], values_[i]);
    (*bst_)[data_[i]] = data_[i];
    (*unordered_map_)[data_[i]] = data_[i];
    (*dense_map_)[data_[i]] = data_[i];
  }

  // Generate non-existent keys
  std::vector<int64_t> missKeys;
  missKeys.reserve(data_.size());
  constexpr int64_t kMissOffset = 1'000'000'000'000LL;
  for (int64_t key : data_) {
    missKeys.push_back(key + kMissOffset);
  }

  std::mt19937 rng(1);

  for (auto _ : state) {
    std::vector<int64_t> shuffled = missKeys;
    std::shuffle(shuffled.begin(), shuffled.end(), rng);

    // 1. ART
    int64_t artMiss = 0;
    auto start = std::chrono::high_resolution_clock::now();
    for (int64_t key : shuffled) {
      if (!art_->Get(key))
        artMiss++;
    }
    auto end = std::chrono::high_resolution_clock::now();
    auto artTime = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();

    // 2. std::map
    int64_t bstMiss = 0;
    start = std::chrono::high_resolution_clock::now();
    for (int64_t key : shuffled) {
      if (bst_->find(key) == bst_->end())
        bstMiss++;
    }
    end = std::chrono::high_resolution_clock::now();
    auto bstTime = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();

    // 3. std::unordered_map
    int64_t uoMiss = 0;
    start = std::chrono::high_resolution_clock::now();
    for (int64_t key : shuffled) {
      if (unordered_map_->find(key) == unordered_map_->end())
        uoMiss++;
    }
    end = std::chrono::high_resolution_clock::now();
    auto uoTime = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();

    // 4. ankerl::unordered_dense
    int64_t deMiss = 0;
    start = std::chrono::high_resolution_clock::now();
    for (int64_t key : shuffled) {
      if (dense_map_->find(key) == dense_map_->end())
        deMiss++;
    }
    end = std::chrono::high_resolution_clock::now();
    auto deTime = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();

    ReportCounters(state, artTime, bstTime, uoTime, deTime);
    state.counters["miss"] =
      benchmark::Counter(artMiss + bstMiss + uoMiss + deMiss, benchmark::Counter::kAvgIterations);
  }
}

BENCHMARK_REGISTER_F(ArtTreeBenchmark, GetMiss)->Iterations(kNumIterations);

// Benchmark: REMOVE - Erase all keys
BENCHMARK_DEFINE_F(ArtTreeBenchmark, Remove)(benchmark::State& state) {
  std::mt19937 rng(1);

  for (auto _ : state) {
    // Pre-populate all structures
    art_->Clear();
    bst_->clear();
    unordered_map_->clear();
    dense_map_->clear();
    set_->clear();
    for (size_t i = 0; i < data_.size(); ++i) {
      art_->Put(data_[i], values_[i]);
      (*bst_)[data_[i]] = data_[i];
      (*unordered_map_)[data_[i]] = data_[i];
      (*dense_map_)[data_[i]] = data_[i];
      set_->insert(data_[i]);
    }

    std::vector<int64_t> shuffled = data_;
    std::shuffle(shuffled.begin(), shuffled.end(), rng);

    // 1. ART
    auto start = std::chrono::high_resolution_clock::now();
    for (int64_t key : shuffled) {
      art_->Remove(key);
    }
    auto end = std::chrono::high_resolution_clock::now();
    auto artTime = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();

    // 2. std::map
    start = std::chrono::high_resolution_clock::now();
    for (int64_t key : shuffled) {
      bst_->erase(key);
    }
    end = std::chrono::high_resolution_clock::now();
    auto bstTime = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();

    // 3. std::unordered_map
    start = std::chrono::high_resolution_clock::now();
    for (int64_t key : shuffled) {
      unordered_map_->erase(key);
    }
    end = std::chrono::high_resolution_clock::now();
    auto uoTime = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();

    // 4. ankerl::unordered_dense
    start = std::chrono::high_resolution_clock::now();
    for (int64_t key : shuffled) {
      dense_map_->erase(key);
    }
    end = std::chrono::high_resolution_clock::now();
    auto deTime = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();

    // 5. std::set
    start = std::chrono::high_resolution_clock::now();
    for (int64_t key : shuffled) {
      set_->erase(key);
    }
    end = std::chrono::high_resolution_clock::now();
    auto setTime = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();

    ReportCounters(state, artTime, bstTime, uoTime, deTime, setTime);
  }
}

BENCHMARK_REGISTER_F(ArtTreeBenchmark, Remove)->Iterations(kNumIterations);

// Benchmark: FOREACH - Ordered iteration (first N elements)
BENCHMARK_DEFINE_F(ArtTreeBenchmark, ForEach)(benchmark::State& state) {
  constexpr int forEachSize = 5000;

  // Pre-populate
  for (size_t i = 0; i < data_.size(); ++i) {
    art_->Put(data_[i], values_[i]);
    (*bst_)[data_[i]] = data_[i];
    (*dense_map_)[data_[i]] = data_[i];
    (*unordered_map_)[data_[i]] = data_[i];
    set_->insert(data_[i]);
  }

  for (auto _ : state) {
    // 1. ART (ordered)
    TestConsumerInt64 artConsumer;
    auto start = std::chrono::high_resolution_clock::now();
    art_->ForEach(&artConsumer, forEachSize);
    auto end = std::chrono::high_resolution_clock::now();
    auto artTime = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();

    // 2. std::map (ordered)
    std::vector<int64_t> bstKeys;
    bstKeys.reserve(forEachSize);
    start = std::chrono::high_resolution_clock::now();
    int count = 0;
    for (const auto& pair : *bst_) {
      if (count >= forEachSize)
        break;
      bstKeys.push_back(pair.first);
      count++;
    }
    end = std::chrono::high_resolution_clock::now();
    auto bstTime = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();

    // 3. std::unordered_map (unordered)
    std::vector<int64_t> uoKeys;
    uoKeys.reserve(forEachSize);
    start = std::chrono::high_resolution_clock::now();
    count = 0;
    for (const auto& pair : *unordered_map_) {
      if (count >= forEachSize)
        break;
      uoKeys.push_back(pair.first);
      count++;
    }
    end = std::chrono::high_resolution_clock::now();
    auto uoTime = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();

    // 4. ankerl::unordered_dense (unordered)
    std::vector<int64_t> deKeys;
    deKeys.reserve(forEachSize);
    start = std::chrono::high_resolution_clock::now();
    count = 0;
    for (const auto& pair : *dense_map_) {
      if (count >= forEachSize)
        break;
      deKeys.push_back(pair.first);
      count++;
    }
    end = std::chrono::high_resolution_clock::now();
    auto deTime = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();

    // 5. std::set (ordered)
    std::vector<int64_t> setKeys;
    setKeys.reserve(forEachSize);
    start = std::chrono::high_resolution_clock::now();
    count = 0;
    for (int64_t key : *set_) {
      if (count >= forEachSize)
        break;
      setKeys.push_back(key);
      count++;
    }
    end = std::chrono::high_resolution_clock::now();
    auto setTime = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();

    ReportCounters(state, artTime, bstTime, uoTime, deTime, setTime);
    state.counters["cnt"] = benchmark::Counter(
      artConsumer.keys.size() + bstKeys.size() + uoKeys.size() + deKeys.size() + setKeys.size(),
      benchmark::Counter::kAvgIterations);
  }
}

BENCHMARK_REGISTER_F(ArtTreeBenchmark, ForEach)->Iterations(kNumIterations);

// Benchmark: FOREACH_DESC - Reverse ordered iteration
BENCHMARK_DEFINE_F(ArtTreeBenchmark, ForEachDesc)(benchmark::State& state) {
  constexpr int forEachSize = 5000;

  // Pre-populate
  for (size_t i = 0; i < data_.size(); ++i) {
    art_->Put(data_[i], values_[i]);
    (*bst_)[data_[i]] = data_[i];
    (*dense_map_)[data_[i]] = data_[i];
    (*unordered_map_)[data_[i]] = data_[i];
    set_->insert(data_[i]);
  }

  for (auto _ : state) {
    // 1. ART (ordered descending)
    TestConsumerInt64 artConsumer;
    auto start = std::chrono::high_resolution_clock::now();
    art_->ForEachDesc(&artConsumer, forEachSize);
    auto end = std::chrono::high_resolution_clock::now();
    auto artTime = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();

    // 2. std::map (ordered descending via rbegin)
    std::vector<int64_t> bstKeys;
    bstKeys.reserve(forEachSize);
    start = std::chrono::high_resolution_clock::now();
    int count = 0;
    for (auto it = bst_->rbegin(); it != bst_->rend() && count < forEachSize; ++it) {
      bstKeys.push_back(it->first);
      count++;
    }
    end = std::chrono::high_resolution_clock::now();
    auto bstTime = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();

    // 3. std::unordered_map (no order)
    std::vector<int64_t> uoKeys;
    uoKeys.reserve(forEachSize);
    start = std::chrono::high_resolution_clock::now();
    count = 0;
    for (const auto& pair : *unordered_map_) {
      if (count >= forEachSize)
        break;
      uoKeys.push_back(pair.first);
      count++;
    }
    end = std::chrono::high_resolution_clock::now();
    auto uoTime = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();

    // 4. ankerl::unordered_dense (no order)
    std::vector<int64_t> deKeys;
    deKeys.reserve(forEachSize);
    start = std::chrono::high_resolution_clock::now();
    count = 0;
    for (const auto& pair : *dense_map_) {
      if (count >= forEachSize)
        break;
      deKeys.push_back(pair.first);
      count++;
    }
    end = std::chrono::high_resolution_clock::now();
    auto deTime = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();

    // 5. std::set (ordered descending via rbegin)
    std::vector<int64_t> setKeys;
    setKeys.reserve(forEachSize);
    start = std::chrono::high_resolution_clock::now();
    count = 0;
    for (auto it = set_->rbegin(); it != set_->rend() && count < forEachSize; ++it) {
      setKeys.push_back(*it);
      count++;
    }
    end = std::chrono::high_resolution_clock::now();
    auto setTime = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();

    ReportCounters(state, artTime, bstTime, uoTime, deTime, setTime);
  }
}

BENCHMARK_REGISTER_F(ArtTreeBenchmark, ForEachDesc)->Iterations(kNumIterations);

// Benchmark: HIGHER (upper_bound - find first key strictly greater than given)
// Only ordered structures support this: ART, std::map, std::set
// Hash maps do NOT support upper_bound
BENCHMARK_DEFINE_F(ArtTreeBenchmark, Higher)(benchmark::State& state) {
  // Pre-populate ordered structures only
  for (size_t i = 0; i < data_.size(); ++i) {
    art_->Put(data_[i], values_[i]);
    (*bst_)[data_[i]] = data_[i];
    set_->insert(data_[i]);
  }

  std::mt19937 rng(1);

  for (auto _ : state) {
    std::vector<int64_t> shuffled = data_;
    std::shuffle(shuffled.begin(), shuffled.end(), rng);

    // 1. ART
    int64_t artSum = 0;
    auto start = std::chrono::high_resolution_clock::now();
    for (int64_t key : shuffled) {
      int64_t* v = art_->GetHigherValue(key);
      if (v)
        artSum += *v;
    }
    auto end = std::chrono::high_resolution_clock::now();
    auto artTime = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();

    // 2. std::map (upper_bound returns key+value)
    int64_t bstSum = 0;
    start = std::chrono::high_resolution_clock::now();
    for (int64_t key : shuffled) {
      auto it = bst_->upper_bound(key);
      if (it != bst_->end())
        bstSum += it->second;
    }
    end = std::chrono::high_resolution_clock::now();
    auto bstTime = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();

    // 3. std::set (upper_bound returns key only)
    int64_t setSum = 0;
    start = std::chrono::high_resolution_clock::now();
    for (int64_t key : shuffled) {
      auto it = set_->upper_bound(key);
      if (it != set_->end())
        setSum += *it;
    }
    end = std::chrono::high_resolution_clock::now();
    auto setTime = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();

    // Report (no hash maps - they don't support ordered ops)
    state.counters["1_art"] = benchmark::Counter(artTime, benchmark::Counter::kAvgIterations);
    state.counters["2_bst"] = benchmark::Counter(bstTime, benchmark::Counter::kAvgIterations);
    state.counters["3_set"] = benchmark::Counter(setTime, benchmark::Counter::kAvgIterations);
    state.counters["vs_bst%"] = benchmark::Counter(
      PercentImprovement(bstTime, artTime) * kNumIterations, benchmark::Counter::kAvgIterations);
    state.counters["vs_set%"] = benchmark::Counter(
      PercentImprovement(setTime, artTime) * kNumIterations, benchmark::Counter::kAvgIterations);
    state.counters["sum"] =
      benchmark::Counter(artSum + bstSum + setSum, benchmark::Counter::kAvgIterations);
  }
}

BENCHMARK_REGISTER_F(ArtTreeBenchmark, Higher)->Iterations(kNumIterations);

// Benchmark: LOWER (find first key strictly less than given)
// Only ordered structures support this: ART, std::map, std::set
// Hash maps do NOT support lower_bound
BENCHMARK_DEFINE_F(ArtTreeBenchmark, Lower)(benchmark::State& state) {
  // Pre-populate ordered structures only
  for (size_t i = 0; i < data_.size(); ++i) {
    art_->Put(data_[i], values_[i]);
    (*bst_)[data_[i]] = data_[i];
    set_->insert(data_[i]);
  }

  std::mt19937 rng(1);

  for (auto _ : state) {
    std::vector<int64_t> shuffled = data_;
    std::shuffle(shuffled.begin(), shuffled.end(), rng);

    // 1. ART
    int64_t artSum = 0;
    auto start = std::chrono::high_resolution_clock::now();
    for (int64_t key : shuffled) {
      int64_t* v = art_->GetLowerValue(key);
      if (v)
        artSum += *v;
    }
    auto end = std::chrono::high_resolution_clock::now();
    auto artTime = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();

    // 2. std::map (lower_bound - 1)
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
    auto bstTime = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();

    // 3. std::set (lower_bound - 1, key only)
    int64_t setSum = 0;
    start = std::chrono::high_resolution_clock::now();
    for (int64_t key : shuffled) {
      auto it = set_->lower_bound(key);
      if (it != set_->begin()) {
        --it;
        setSum += *it;
      }
    }
    end = std::chrono::high_resolution_clock::now();
    auto setTime = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();

    // Report (no hash maps - they don't support ordered ops)
    state.counters["1_art"] = benchmark::Counter(artTime, benchmark::Counter::kAvgIterations);
    state.counters["2_bst"] = benchmark::Counter(bstTime, benchmark::Counter::kAvgIterations);
    state.counters["3_set"] = benchmark::Counter(setTime, benchmark::Counter::kAvgIterations);
    state.counters["vs_bst%"] = benchmark::Counter(
      PercentImprovement(bstTime, artTime) * kNumIterations, benchmark::Counter::kAvgIterations);
    state.counters["vs_set%"] = benchmark::Counter(
      PercentImprovement(setTime, artTime) * kNumIterations, benchmark::Counter::kAvgIterations);
    state.counters["sum"] =
      benchmark::Counter(artSum + bstSum + setSum, benchmark::Counter::kAvgIterations);
  }
}

BENCHMARK_REGISTER_F(ArtTreeBenchmark, Lower)->Iterations(kNumIterations);

BENCHMARK_MAIN();
