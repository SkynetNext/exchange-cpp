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

#include <algorithm>
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
#include <vector>

using namespace exchange::core::collections::art;
using namespace exchange::core::collections::objpool;

// Default iterations for benchmarks
constexpr int kNumIterations = 10;

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
    int64_t highestBit = i == 0 ? 0 : (1LL << (63 - __builtin_clzll(i)));
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
    art_ = new LongAdaptiveRadixTreeMap<int64_t>(objectsPool_);
    bst_ = new std::map<int64_t, int64_t>();

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
    delete objectsPool_;
  }

  ObjectsPool *objectsPool_;
  LongAdaptiveRadixTreeMap<int64_t> *art_;
  std::map<int64_t, int64_t> *bst_;
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
BENCHMARK_DEFINE_F(ArtTreeBenchmark, Put)(benchmark::State &state) {
  std::mt19937 rng(1);

  for (auto _ : state) {
    // Clear
    art_->Clear();
    bst_->clear();

    // Shuffle data
    std::vector<int64_t> shuffled = data_;
    std::shuffle(shuffled.begin(), shuffled.end(), rng);

    // Measure ART
    auto start = std::chrono::high_resolution_clock::now();
    for (size_t i = 0; i < shuffled.size(); ++i) {
      art_->Put(shuffled[i], values_[i]);
    }
    auto end = std::chrono::high_resolution_clock::now();
    auto artTime =
        std::chrono::duration_cast<std::chrono::nanoseconds>(end - start)
            .count();

    // Measure BST
    start = std::chrono::high_resolution_clock::now();
    for (size_t i = 0; i < shuffled.size(); ++i) {
      (*bst_)[shuffled[i]] = shuffled[i];
    }
    end = std::chrono::high_resolution_clock::now();
    auto bstTime =
        std::chrono::duration_cast<std::chrono::nanoseconds>(end - start)
            .count();

    state.counters["art_ns"] =
        benchmark::Counter(artTime, benchmark::Counter::kAvgIterations);
    state.counters["bst_ns"] =
        benchmark::Counter(bstTime, benchmark::Counter::kAvgIterations);
    // improvement: kAvgIterations will divide by kNumIterations, so multiply by
    // it to compensate
    state.counters["improvement"] = benchmark::Counter(
        PercentImprovement(bstTime, artTime) * kNumIterations,
        benchmark::Counter::kAvgIterations);
  }
}
BENCHMARK_REGISTER_F(ArtTreeBenchmark, Put)->Iterations(kNumIterations);

// Benchmark: GET_HIT
BENCHMARK_DEFINE_F(ArtTreeBenchmark, GetHit)(benchmark::State &state) {
  // Pre-populate
  for (size_t i = 0; i < data_.size(); ++i) {
    art_->Put(data_[i], values_[i]);
    (*bst_)[data_[i]] = data_[i];
  }

  std::mt19937 rng(1);

  for (auto _ : state) {
    // Shuffle data
    std::vector<int64_t> shuffled = data_;
    std::shuffle(shuffled.begin(), shuffled.end(), rng);

    // Measure ART
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

    // Measure BST
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

    state.counters["art_ns"] =
        benchmark::Counter(artTime, benchmark::Counter::kAvgIterations);
    state.counters["bst_ns"] =
        benchmark::Counter(bstTime, benchmark::Counter::kAvgIterations);
    // improvement: kAvgIterations will divide by kNumIterations, so multiply by
    // it to compensate
    state.counters["improvement"] = benchmark::Counter(
        PercentImprovement(bstTime, artTime) * kNumIterations,
        benchmark::Counter::kAvgIterations);
    state.counters["art_sum"] =
        benchmark::Counter(artSum, benchmark::Counter::kAvgIterations);
    state.counters["bst_sum"] =
        benchmark::Counter(bstSum, benchmark::Counter::kAvgIterations);
  }
}
BENCHMARK_REGISTER_F(ArtTreeBenchmark, GetHit)->Iterations(kNumIterations);

// Benchmark: REMOVE
BENCHMARK_DEFINE_F(ArtTreeBenchmark, Remove)(benchmark::State &state) {
  std::mt19937 rng(1);

  for (auto _ : state) {
    // Pre-populate
    art_->Clear();
    bst_->clear();
    for (size_t i = 0; i < data_.size(); ++i) {
      art_->Put(data_[i], values_[i]);
      (*bst_)[data_[i]] = data_[i];
    }

    // Shuffle data
    std::vector<int64_t> shuffled = data_;
    std::shuffle(shuffled.begin(), shuffled.end(), rng);

    // Measure ART
    auto start = std::chrono::high_resolution_clock::now();
    for (int64_t key : shuffled) {
      art_->Remove(key);
    }
    auto end = std::chrono::high_resolution_clock::now();
    auto artTime =
        std::chrono::duration_cast<std::chrono::nanoseconds>(end - start)
            .count();

    // Measure BST
    start = std::chrono::high_resolution_clock::now();
    for (int64_t key : shuffled) {
      bst_->erase(key);
    }
    end = std::chrono::high_resolution_clock::now();
    auto bstTime =
        std::chrono::duration_cast<std::chrono::nanoseconds>(end - start)
            .count();

    state.counters["art_ns"] =
        benchmark::Counter(artTime, benchmark::Counter::kAvgIterations);
    state.counters["bst_ns"] =
        benchmark::Counter(bstTime, benchmark::Counter::kAvgIterations);
    // improvement: kAvgIterations will divide by kNumIterations, so multiply by
    // it to compensate
    state.counters["improvement"] = benchmark::Counter(
        PercentImprovement(bstTime, artTime) * kNumIterations,
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

    state.counters["art_ns"] =
        benchmark::Counter(artTime, benchmark::Counter::kAvgIterations);
    state.counters["bst_ns"] =
        benchmark::Counter(bstTime, benchmark::Counter::kAvgIterations);
    // improvement: kAvgIterations will divide by kNumIterations, so multiply by
    // it to compensate
    state.counters["improvement"] = benchmark::Counter(
        PercentImprovement(bstTime, artTime) * kNumIterations,
        benchmark::Counter::kAvgIterations);
    state.counters["art_count"] = benchmark::Counter(
        artConsumer.keys.size(), benchmark::Counter::kAvgIterations);
    state.counters["bst_count"] =
        benchmark::Counter(bstKeys.size(), benchmark::Counter::kAvgIterations);
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

    state.counters["art_ns"] =
        benchmark::Counter(artTime, benchmark::Counter::kAvgIterations);
    state.counters["bst_ns"] =
        benchmark::Counter(bstTime, benchmark::Counter::kAvgIterations);
    // improvement: kAvgIterations will divide by kNumIterations, so multiply by
    // it to compensate
    state.counters["improvement"] = benchmark::Counter(
        PercentImprovement(bstTime, artTime) * kNumIterations,
        benchmark::Counter::kAvgIterations);
  }
}
BENCHMARK_REGISTER_F(ArtTreeBenchmark, ForEachDesc)->Iterations(kNumIterations);

// Benchmark: HIGHER
BENCHMARK_DEFINE_F(ArtTreeBenchmark, Higher)(benchmark::State &state) {
  // Pre-populate
  for (size_t i = 0; i < data_.size(); ++i) {
    art_->Put(data_[i], values_[i]);
    (*bst_)[data_[i]] = data_[i];
  }

  std::mt19937 rng(1);

  for (auto _ : state) {
    // Shuffle data
    std::vector<int64_t> shuffled = data_;
    std::shuffle(shuffled.begin(), shuffled.end(), rng);

    // Measure ART
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

    // Measure BST
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

    state.counters["art_ns"] =
        benchmark::Counter(artTime, benchmark::Counter::kAvgIterations);
    state.counters["bst_ns"] =
        benchmark::Counter(bstTime, benchmark::Counter::kAvgIterations);
    // improvement: kAvgIterations will divide by kNumIterations, so multiply by
    // it to compensate
    state.counters["improvement"] = benchmark::Counter(
        PercentImprovement(bstTime, artTime) * kNumIterations,
        benchmark::Counter::kAvgIterations);
    state.counters["art_sum"] =
        benchmark::Counter(artSum, benchmark::Counter::kAvgIterations);
    state.counters["bst_sum"] =
        benchmark::Counter(bstSum, benchmark::Counter::kAvgIterations);
  }
}
BENCHMARK_REGISTER_F(ArtTreeBenchmark, Higher)->Iterations(kNumIterations);

// Benchmark: LOWER
BENCHMARK_DEFINE_F(ArtTreeBenchmark, Lower)(benchmark::State &state) {
  // Pre-populate
  for (size_t i = 0; i < data_.size(); ++i) {
    art_->Put(data_[i], values_[i]);
    (*bst_)[data_[i]] = data_[i];
  }

  std::mt19937 rng(1);

  for (auto _ : state) {
    // Shuffle data
    std::vector<int64_t> shuffled = data_;
    std::shuffle(shuffled.begin(), shuffled.end(), rng);

    // Measure ART
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

    // Measure BST
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

    state.counters["art_ns"] =
        benchmark::Counter(artTime, benchmark::Counter::kAvgIterations);
    state.counters["bst_ns"] =
        benchmark::Counter(bstTime, benchmark::Counter::kAvgIterations);
    // improvement: kAvgIterations will divide by kNumIterations, so multiply by
    // it to compensate
    state.counters["improvement"] = benchmark::Counter(
        PercentImprovement(bstTime, artTime) * kNumIterations,
        benchmark::Counter::kAvgIterations);
    state.counters["art_sum"] =
        benchmark::Counter(artSum, benchmark::Counter::kAvgIterations);
    state.counters["bst_sum"] =
        benchmark::Counter(bstSum, benchmark::Counter::kAvgIterations);
  }
}
BENCHMARK_REGISTER_F(ArtTreeBenchmark, Lower)->Iterations(kNumIterations);

BENCHMARK_MAIN();
