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

/**
 * Java-aligned ART vs BST benchmark: same per-iteration flow as
 * reference/collections PerfLongAdaptiveRadixTreeMap.shouldLoadManyItems().
 * No Google Benchmark — plain loop + AVERAGE-style output for perf/cache
 * comparison with Java.
 */

#include <exchange/core/collections/art/LongAdaptiveRadixTreeMap.h>
#include <exchange/core/collections/art/LongObjConsumer.h>
#include <exchange/core/collections/objpool/ObjectsPool.h>
#include <algorithm>
#include <chrono>
#include <climits>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <map>
#include <random>
#include <stdexcept>
#include <vector>

#ifdef _WIN32
#  include <mimalloc-new-delete.h>
#endif

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

enum class Bm {
  BST_PUT,
  BST_GET_HIT,
  BST_REMOVE,
  BST_FOREACH,
  BST_FOREACH_DESC,
  BST_HIGHER,
  BST_LOWER,
  ART_PUT,
  ART_GET_HIT,
  ART_REMOVE,
  ART_FOREACH,
  ART_FOREACH_DESC,
  ART_HIGHER,
  ART_LOWER,
};

static void
ExecuteInRandomOrder(std::mt19937& rng, std::function<void()> a, std::function<void()> b) {
  if (rng() % 2 == 0) {
    a();
    b();
  } else {
    b();
    a();
  }
}

static int StepFunction(std::mt19937& rng, int i) {
  int64_t hi = i == 0 ? 0 : (1LL << (63 - CountLeadingZeros64(static_cast<uint64_t>(i))));
  int maxStep = static_cast<int>(
    std::min(static_cast<int64_t>(INT_MAX), static_cast<int64_t>(1LL + (hi >> 8))));
  std::uniform_int_distribution<int> dist(0, maxStep);
  return 1 + dist(rng);
}

static std::vector<int64_t> GenerateList(std::mt19937& rng, int num, int64_t offset) {
  std::vector<int64_t> list;
  list.reserve(num);
  int64_t j = 0;
  for (int i = 0; i < num; i++) {
    list.push_back(offset + j);
    j += StepFunction(rng, i);
  }
  std::shuffle(list.begin(), list.end(), rng);
  return list;
}

static int PercentImprovement(int64_t bstNs, int64_t artNs) {
  if (artNs == 0)
    return 0;
  return static_cast<int>(100.0 * (static_cast<double>(bstNs) / static_cast<double>(artNs) - 1.0));
}

static float NanoToMs(int64_t ns) {
  return static_cast<float>(ns / 1000) / 1000.0f;
}

// ART returns V*; we compare with BST value. EntriesList is (key, V*).
static void CheckStreamsEqual(LongAdaptiveRadixTreeMap<int64_t>& art,
                              std::map<int64_t, int64_t>& bst) {
  auto artList = art.EntriesList();
  auto bstIt = bst.begin();
  auto bstEnd = bst.end();
  for (const auto& p : artList) {
    if (bstIt == bstEnd || bstIt->first != p.first || bstIt->second != *p.second)
      throw std::runtime_error("CheckStreamsEqual mismatch");
    ++bstIt;
  }
  if (bstIt != bstEnd)
    throw std::runtime_error("CheckStreamsEqual size mismatch");
}

int main(int argc, char** argv) {
  const int numIters = (argc >= 2) ? std::atoi(argv[1]) : 3;
  const int num = 5'000'000;
  const int forEachSize = 5000;

  std::mt19937 rng(1);
  std::map<Bm, std::vector<int64_t>> times;

  auto addTime = [&times](Bm b, int64_t ns) { times[b].push_back(ns); };
  auto avgNs = [&times](Bm b) -> int64_t {
    auto& v = times[b];
    if (v.empty())
      return 0;
    int64_t sum = 0;
    for (int64_t x : v)
      sum += x;
    return sum / static_cast<int64_t>(v.size());
  };

  for (int iter = 0; iter < numIters; iter++) {
    ObjectsPool* pool = ObjectsPool::CreateDefaultTestPool();
    auto* art = new LongAdaptiveRadixTreeMap<int64_t>(pool);
    std::map<int64_t, int64_t> bst;

    int64_t offset = 1'000'000'000LL + (rng() % 1'000'000);
    std::vector<int64_t> list = GenerateList(rng, num, offset);

    std::vector<int64_t*> values;
    values.reserve(list.size());
    for (size_t i = 0; i < list.size(); i++) {
      values.push_back(new int64_t(list[i]));
    }

    // Put (random order)
    ExecuteInRandomOrder(
      rng,
      [&] {
        auto t0 = std::chrono::high_resolution_clock::now();
        for (int64_t x : list)
          bst[x] = x;
        addTime(Bm::BST_PUT, std::chrono::duration_cast<std::chrono::nanoseconds>(
                               std::chrono::high_resolution_clock::now() - t0)
                               .count());
      },
      [&] {
        auto t0 = std::chrono::high_resolution_clock::now();
        for (size_t i = 0; i < list.size(); i++)
          art->Put(list[i], values[i]);
        addTime(Bm::ART_PUT, std::chrono::duration_cast<std::chrono::nanoseconds>(
                               std::chrono::high_resolution_clock::now() - t0)
                               .count());
      });

    std::shuffle(list.begin(), list.end(), rng);

    // GetHit (random order)
    ExecuteInRandomOrder(
      rng,
      [&] {
        int64_t sum = 0;
        auto t0 = std::chrono::high_resolution_clock::now();
        for (int64_t x : list)
          sum += bst.at(x);
        addTime(Bm::BST_GET_HIT, std::chrono::duration_cast<std::chrono::nanoseconds>(
                                   std::chrono::high_resolution_clock::now() - t0)
                                   .count());
        (void)sum;
      },
      [&] {
        int64_t sum = 0;
        auto t0 = std::chrono::high_resolution_clock::now();
        for (int64_t x : list) {
          int64_t* v = art->Get(x);
          if (v)
            sum += *v;
        }
        addTime(Bm::ART_GET_HIT, std::chrono::duration_cast<std::chrono::nanoseconds>(
                                   std::chrono::high_resolution_clock::now() - t0)
                                   .count());
        (void)sum;
      });

    art->ValidateInternalState();
    CheckStreamsEqual(*art, bst);

    std::shuffle(list.begin(), list.end(), rng);

    // Higher (random order): first = ART, second = BST (same as Java)
    ExecuteInRandomOrder(
      rng,
      [&] {
        int64_t sum = 0;
        auto t0 = std::chrono::high_resolution_clock::now();
        for (int64_t x : list) {
          int64_t* v = art->GetHigherValue(x);
          if (v)
            sum += *v;
        }
        addTime(Bm::ART_HIGHER, std::chrono::duration_cast<std::chrono::nanoseconds>(
                                  std::chrono::high_resolution_clock::now() - t0)
                                  .count());
        (void)sum;
      },
      [&] {
        int64_t sum = 0;
        auto t0 = std::chrono::high_resolution_clock::now();
        for (int64_t x : list) {
          auto it = bst.upper_bound(x);
          if (it != bst.end())
            sum += it->second;
        }
        addTime(Bm::BST_HIGHER, std::chrono::duration_cast<std::chrono::nanoseconds>(
                                  std::chrono::high_resolution_clock::now() - t0)
                                  .count());
        (void)sum;
      });

    // Lower (random order)
    ExecuteInRandomOrder(
      rng,
      [&] {
        int64_t sum = 0;
        auto t0 = std::chrono::high_resolution_clock::now();
        for (int64_t x : list) {
          int64_t* v = art->GetLowerValue(x);
          if (v)
            sum += *v;
        }
        addTime(Bm::ART_LOWER, std::chrono::duration_cast<std::chrono::nanoseconds>(
                                 std::chrono::high_resolution_clock::now() - t0)
                                 .count());
        (void)sum;
      },
      [&] {
        int64_t sum = 0;
        auto t0 = std::chrono::high_resolution_clock::now();
        for (int64_t x : list) {
          auto it = bst.lower_bound(x);
          if (it != bst.begin())
            sum += (--it)->second;
        }
        addTime(Bm::BST_LOWER, std::chrono::duration_cast<std::chrono::nanoseconds>(
                                 std::chrono::high_resolution_clock::now() - t0)
                                 .count());
        (void)sum;
      });

    // validate getHigher / getLower
    for (int64_t x : list) {
      int64_t* v1 = art->GetHigherValue(x);
      auto it = bst.upper_bound(x);
      int64_t v2 = (it != bst.end()) ? it->second : 0;
      int64_t a1 = v1 ? *v1 : 0;
      if (a1 != v2)
        throw std::runtime_error("getHigherValue mismatch");
    }
    for (int64_t x : list) {
      int64_t* v1 = art->GetLowerValue(x);
      auto it = bst.lower_bound(x);
      int64_t v2 = (it != bst.begin()) ? (--it)->second : 0;
      int64_t a1 = v1 ? *v1 : 0;
      if (a1 != v2)
        throw std::runtime_error("getLowerValue mismatch");
    }

    // ForEach (random order) + validate
    std::vector<int64_t> artKeys, artVals, bstKeys, bstVals;
    artKeys.reserve(forEachSize);
    artVals.reserve(forEachSize);
    bstKeys.reserve(forEachSize);
    bstVals.reserve(forEachSize);
    auto artConsumer = [&artKeys, &artVals](int64_t k, int64_t* v) {
      artKeys.push_back(k);
      artVals.push_back(v ? *v : 0);
    };
    ExecuteInRandomOrder(
      rng,
      [&] {
        auto t0 = std::chrono::high_resolution_clock::now();
        int n = 0;
        for (const auto& e : bst) {
          if (n >= forEachSize)
            break;
          bstKeys.push_back(e.first);
          bstVals.push_back(e.second);
          n++;
        }
        addTime(Bm::BST_FOREACH, std::chrono::duration_cast<std::chrono::nanoseconds>(
                                   std::chrono::high_resolution_clock::now() - t0)
                                   .count());
      },
      [&] {
        artKeys.clear();
        artVals.clear();
        auto t0 = std::chrono::high_resolution_clock::now();
        art->ForEach(artConsumer, forEachSize);
        addTime(Bm::ART_FOREACH, std::chrono::duration_cast<std::chrono::nanoseconds>(
                                   std::chrono::high_resolution_clock::now() - t0)
                                   .count());
      });
    if (artKeys != bstKeys || artVals != bstVals)
      throw std::runtime_error("forEach validate mismatch");
    artKeys.clear();
    artVals.clear();
    bstKeys.clear();
    bstVals.clear();

    // ForEachDesc (random order) + validate
    ExecuteInRandomOrder(
      rng,
      [&] {
        auto t0 = std::chrono::high_resolution_clock::now();
        int n = 0;
        for (auto it = bst.rbegin(); it != bst.rend() && n < forEachSize; ++it, n++) {
          bstKeys.push_back(it->first);
          bstVals.push_back(it->second);
        }
        addTime(Bm::BST_FOREACH_DESC, std::chrono::duration_cast<std::chrono::nanoseconds>(
                                        std::chrono::high_resolution_clock::now() - t0)
                                        .count());
      },
      [&] {
        artKeys.clear();
        artVals.clear();
        auto t0 = std::chrono::high_resolution_clock::now();
        art->ForEachDesc(artConsumer, forEachSize);
        addTime(Bm::ART_FOREACH_DESC, std::chrono::duration_cast<std::chrono::nanoseconds>(
                                        std::chrono::high_resolution_clock::now() - t0)
                                        .count());
      });
    if (artKeys != bstKeys || artVals != bstVals)
      throw std::runtime_error("forEachDesc validate mismatch");

    // Remove (random order)
    ExecuteInRandomOrder(
      rng,
      [&] {
        auto t0 = std::chrono::high_resolution_clock::now();
        for (int64_t x : list)
          bst.erase(x);
        addTime(Bm::BST_REMOVE, std::chrono::duration_cast<std::chrono::nanoseconds>(
                                  std::chrono::high_resolution_clock::now() - t0)
                                  .count());
      },
      [&] {
        auto t0 = std::chrono::high_resolution_clock::now();
        for (int64_t x : list)
          art->Remove(x);
        addTime(Bm::ART_REMOVE, std::chrono::duration_cast<std::chrono::nanoseconds>(
                                  std::chrono::high_resolution_clock::now() - t0)
                                  .count());
      });

    art->ValidateInternalState();
    CheckStreamsEqual(*art, bst);

    // Java: remove oldest half every 2 iters (then log AVERAGE this iter)
    if (iter % 2 == 1) {
      for (auto& kv : times) {
        if (!kv.second.empty())
          kv.second.erase(kv.second.begin());
      }
    }

    // Per-iteration AVERAGE lines (same as Java log.info after each iter)
    int64_t bstPut = avgNs(Bm::BST_PUT), artPut = avgNs(Bm::ART_PUT);
    int64_t bstGet = avgNs(Bm::BST_GET_HIT), artGet = avgNs(Bm::ART_GET_HIT);
    int64_t bstRm = avgNs(Bm::BST_REMOVE), artRm = avgNs(Bm::ART_REMOVE);
    int64_t bstFe = avgNs(Bm::BST_FOREACH), artFe = avgNs(Bm::ART_FOREACH);
    int64_t bstFd = avgNs(Bm::BST_FOREACH_DESC), artFd = avgNs(Bm::ART_FOREACH_DESC);
    int64_t bstHi = avgNs(Bm::BST_HIGHER), artHi = avgNs(Bm::ART_HIGHER);
    int64_t bstLo = avgNs(Bm::BST_LOWER), artLo = avgNs(Bm::ART_LOWER);
    std::printf("AVERAGE PUT    BST %.3fms ADT %.3fms (%d%%)\n", NanoToMs(bstPut), NanoToMs(artPut),
                PercentImprovement(bstPut, artPut));
    std::printf("AVERAGE GETHIT BST %.3fms ADT %.3fms (%d%%)\n", NanoToMs(bstGet), NanoToMs(artGet),
                PercentImprovement(bstGet, artGet));
    std::printf("AVERAGE REMOVE BST %.3fms ADT %.3fms (%d%%)\n", NanoToMs(bstRm), NanoToMs(artRm),
                PercentImprovement(bstRm, artRm));
    std::printf("AVERAGE FOREACH BST %.3fms ADT %.3fms (%d%%)\n", NanoToMs(bstFe), NanoToMs(artFe),
                PercentImprovement(bstFe, artFe));
    std::printf("AVERAGE FOREACH DESC BST %.3fms ADT %.3fms (%d%%)\n", NanoToMs(bstFd),
                NanoToMs(artFd), PercentImprovement(bstFd, artFd));
    std::printf("AVERAGE HIGHER BST %.3fms ADT %.3fms (%d%%)\n", NanoToMs(bstHi), NanoToMs(artHi),
                PercentImprovement(bstHi, artHi));
    std::printf("AVERAGE LOWER BST %.3fms ADT %.3fms (%d%%)\n", NanoToMs(bstLo), NanoToMs(artLo),
                PercentImprovement(bstLo, artLo));

    for (int64_t* v : values)
      delete v;
    delete art;
    delete pool;
  }

  std::printf("---------------------------------------\n");

  return 0;
}
