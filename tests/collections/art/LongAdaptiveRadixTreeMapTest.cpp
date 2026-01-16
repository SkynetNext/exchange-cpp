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

#include <exchange/core/collections/art/LongAdaptiveRadixTreeMap.h>
#include <exchange/core/collections/art/LongObjConsumer.h>
#include <gtest/gtest.h>
#include <map>
#include <random>
#include <vector>

using namespace exchange::core::collections::art;

// Helper class to collect key-value pairs for std::string
class TestConsumer : public LongObjConsumer<std::string> {
public:
  std::vector<int64_t> keys;
  std::vector<std::string *> values;

  void Accept(int64_t key, std::string *value) override {
    keys.push_back(key);
    values.push_back(value);
  }

  void Clear() {
    keys.clear();
    values.clear();
  }
};

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

class LongAdaptiveRadixTreeMapTest : public ::testing::Test {
protected:
  void SetUp() override {
    map_ = new LongAdaptiveRadixTreeMap<std::string>();
    origMap_.clear();
  }

  void TearDown() override {
    // Clean up string values
    for (auto &pair : origMap_) {
      delete pair.second;
    }
    delete map_;
  }

  void Put(int64_t key, const std::string &value) {
    std::string *strValue = new std::string(value);
    map_->Put(key, strValue);
    map_->ValidateInternalState();

    // Update reference map
    if (origMap_.find(key) != origMap_.end()) {
      delete origMap_[key];
    }
    origMap_[key] = strValue;

    // Verify entries match
    CheckEntriesEqual();
  }

  void Remove(int64_t key) {
    map_->Remove(key);
    map_->ValidateInternalState();

    // Update reference map
    if (origMap_.find(key) != origMap_.end()) {
      delete origMap_[key];
      origMap_.erase(key);
    }

    // Verify entries match
    CheckEntriesEqual();
  }

  void CheckEntriesEqual() {
    auto artEntries = map_->EntriesList();
    auto artIt = artEntries.begin();

    for (const auto &origEntry : origMap_) {
      ASSERT_NE(artIt, artEntries.end())
          << "ART tree has fewer entries than expected";
      EXPECT_EQ(artIt->first, origEntry.first)
          << "Key mismatch: expected " << origEntry.first << ", got "
          << artIt->first;
      EXPECT_EQ(*artIt->second, *origEntry.second)
          << "Value mismatch for key " << origEntry.first;
      ++artIt;
    }
    ASSERT_EQ(artIt, artEntries.end())
        << "ART tree has more entries than expected";
  }

  LongAdaptiveRadixTreeMap<std::string> *map_;
  std::map<int64_t, std::string *> origMap_;
};

TEST_F(LongAdaptiveRadixTreeMapTest, ShouldPerformBasicOperations) {
  map_->ValidateInternalState();
  EXPECT_EQ(map_->Get(0), nullptr);

  Put(2, "two");
  map_->ValidateInternalState();

  Put(223, "dds");
  Put(49, "fn");
  Put(1, "fn");
  Put(INT64_MAX, "fn");
  Put(11239847219LL, "11239847219L");
  Put(1123909LL, "1123909L");
  Put(11239837212LL, "11239837212L");
  Put(13213, "13213");
  Put(13423, "13423");

  EXPECT_EQ(*map_->Get(223), "dds");
  EXPECT_EQ(*map_->Get(INT64_MAX), "fn");
  EXPECT_EQ(*map_->Get(11239837212LL), "11239837212L");
}

TEST_F(LongAdaptiveRadixTreeMapTest, ShouldCallForEach) {
  Put(533, "533");
  Put(573, "573");
  Put(38234, "38234");
  Put(38251, "38251");
  Put(38255, "38255");
  Put(40001, "40001");
  Put(40021, "40021");
  Put(40023, "40023");

  TestConsumer consumer;
  std::vector<int64_t> expectedKeys = {533,   573,   38234, 38251,
                                       38255, 40001, 40021, 40023};
  std::vector<std::string> expectedValues = {
      "533", "573", "38234", "38251", "38255", "40001", "40021", "40023"};

  // Test forEach with unlimited
  map_->ForEach(&consumer, INT_MAX);
  ASSERT_EQ(consumer.keys.size(), expectedKeys.size());
  for (size_t i = 0; i < expectedKeys.size(); i++) {
    EXPECT_EQ(consumer.keys[i], expectedKeys[i]);
    EXPECT_EQ(*consumer.values[i], expectedValues[i]);
  }

  consumer.Clear();

  // Test forEach with limit 8
  map_->ForEach(&consumer, 8);
  ASSERT_EQ(consumer.keys.size(), expectedKeys.size());
  for (size_t i = 0; i < expectedKeys.size(); i++) {
    EXPECT_EQ(consumer.keys[i], expectedKeys[i]);
    EXPECT_EQ(*consumer.values[i], expectedValues[i]);
  }

  consumer.Clear();

  // Test forEach with limit 3
  map_->ForEach(&consumer, 3);
  ASSERT_EQ(consumer.keys.size(), 3u);
  for (size_t i = 0; i < 3; i++) {
    EXPECT_EQ(consumer.keys[i], expectedKeys[i]);
    EXPECT_EQ(*consumer.values[i], expectedValues[i]);
  }

  consumer.Clear();

  // Test forEach with limit 0
  map_->ForEach(&consumer, 0);
  EXPECT_TRUE(consumer.keys.empty());

  consumer.Clear();

  // Test forEachDesc with unlimited
  std::vector<int64_t> expectedKeysRev(expectedKeys.rbegin(),
                                       expectedKeys.rend());
  std::vector<std::string> expectedValuesRev(expectedValues.rbegin(),
                                             expectedValues.rend());

  map_->ForEachDesc(&consumer, INT_MAX);
  ASSERT_EQ(consumer.keys.size(), expectedKeysRev.size());
  for (size_t i = 0; i < expectedKeysRev.size(); i++) {
    EXPECT_EQ(consumer.keys[i], expectedKeysRev[i]);
    EXPECT_EQ(*consumer.values[i], expectedValuesRev[i]);
  }

  consumer.Clear();

  // Test forEachDesc with limit 8
  map_->ForEachDesc(&consumer, 8);
  ASSERT_EQ(consumer.keys.size(), expectedKeysRev.size());
  for (size_t i = 0; i < expectedKeysRev.size(); i++) {
    EXPECT_EQ(consumer.keys[i], expectedKeysRev[i]);
    EXPECT_EQ(*consumer.values[i], expectedValuesRev[i]);
  }

  consumer.Clear();

  // Test forEachDesc with limit 3
  map_->ForEachDesc(&consumer, 3);
  ASSERT_EQ(consumer.keys.size(), 3u);
  for (size_t i = 0; i < 3; i++) {
    EXPECT_EQ(consumer.keys[i], expectedKeysRev[i]);
    EXPECT_EQ(*consumer.values[i], expectedValuesRev[i]);
  }

  consumer.Clear();

  // Test forEachDesc with limit 0
  map_->ForEachDesc(&consumer, 0);
  EXPECT_TRUE(consumer.keys.empty());
}

TEST_F(LongAdaptiveRadixTreeMapTest, ShouldFindHigherKeys) {
  Put(33, "33");
  Put(273, "273");
  Put(182736400230LL, "182736400230");
  Put(182736487234LL, "182736487234");
  Put(37, "37");

  for (int x = 37; x < 273; x++) {
    std::string *result = map_->GetHigherValue(x);
    ASSERT_NE(result, nullptr);
    EXPECT_EQ(*result, "273");
  }

  for (int x = 273; x < 100000; x++) {
    std::string *result = map_->GetHigherValue(x);
    ASSERT_NE(result, nullptr);
    EXPECT_EQ(*result, "182736400230");
  }

  std::string *result = map_->GetHigherValue(182736388198LL);
  ASSERT_NE(result, nullptr);
  EXPECT_EQ(*result, "182736400230");

  for (int64_t x = 182736300230LL; x < 182736400229LL; x++) {
    std::string *result = map_->GetHigherValue(x);
    ASSERT_NE(result, nullptr);
    EXPECT_EQ(*result, "182736400230");
  }

  for (int64_t x = 182736400230LL; x < 182736487234LL; x++) {
    std::string *result = map_->GetHigherValue(x);
    ASSERT_NE(result, nullptr);
    EXPECT_EQ(*result, "182736487234");
  }

  for (int64_t x = 182736487234LL; x < 182736497234LL; x++) {
    EXPECT_EQ(map_->GetHigherValue(x), nullptr);
  }
}

TEST_F(LongAdaptiveRadixTreeMapTest, ShouldFindLowerKeys) {
  Put(33, "33");
  Put(273, "273");
  Put(182736400230LL, "182736400230");
  Put(182736487234LL, "182736487234");
  Put(37, "37");

  std::string *result = map_->GetLowerValue(63120LL);
  ASSERT_NE(result, nullptr);
  EXPECT_EQ(*result, "273");

  result = map_->GetLowerValue(255);
  ASSERT_NE(result, nullptr);
  EXPECT_EQ(*result, "37");

  result = map_->GetLowerValue(275);
  ASSERT_NE(result, nullptr);
  EXPECT_EQ(*result, "273");

  EXPECT_EQ(map_->GetLowerValue(33), nullptr);
  EXPECT_EQ(map_->GetLowerValue(32), nullptr);

  for (int x = 34; x <= 37; x++) {
    result = map_->GetLowerValue(x);
    ASSERT_NE(result, nullptr);
    EXPECT_EQ(*result, "33");
  }

  for (int x = 38; x <= 273; x++) {
    result = map_->GetLowerValue(x);
    ASSERT_NE(result, nullptr);
    EXPECT_EQ(*result, "37");
  }

  for (int x = 274; x < 100000; x++) {
    result = map_->GetLowerValue(x);
    ASSERT_NE(result, nullptr);
    EXPECT_EQ(*result, "273");
  }

  result = map_->GetLowerValue(182736487334LL);
  ASSERT_NE(result, nullptr);
  EXPECT_EQ(*result, "182736487234");

  for (int64_t x = 182736300230LL; x < 182736400230LL; x++) {
    result = map_->GetLowerValue(x);
    ASSERT_NE(result, nullptr);
    EXPECT_EQ(*result, "273");
  }
}

TEST_F(LongAdaptiveRadixTreeMapTest, ShouldCompactNodes) {
  Put(2, "2");
  EXPECT_EQ(*map_->Get(2), "2");
  EXPECT_EQ(map_->Get(3), nullptr);
  EXPECT_EQ(map_->Get(256 + 2), nullptr);
  EXPECT_EQ(map_->Get(256LL * 256 * 256 + 2), nullptr);
  EXPECT_EQ(map_->Get(INT64_MAX - 0xFF + 2), nullptr);

  Put(0x414F32LL, "0x414F32");
  Put(0x414F33LL, "0x414F33");
  Put(0x414E00LL, "0x414E00");
  Put(0x407654LL, "0x407654");
  Put(0x33558822DD44AA11LL, "0x33558822DD44AA11");
  Put(0xFFFFFFFFFFFFFFLL, "0xFFFFFFFFFFFFFF");
  Put(0xFFFFFFFFFFFFFELL, "0xFFFFFFFFFFFFFE");
  Put(0x112233445566LL, "0x112233445566");
  Put(0x1122AAEE5566LL, "0x1122AAEE5566");

  EXPECT_EQ(*map_->Get(0x414F32LL), "0x414F32");
  EXPECT_EQ(*map_->Get(0x414F33LL), "0x414F33");
  EXPECT_EQ(*map_->Get(0x414E00LL), "0x414E00");
  EXPECT_EQ(map_->Get(0x414D00LL), nullptr);
  EXPECT_EQ(map_->Get(0x414D33LL), nullptr);
  EXPECT_EQ(map_->Get(0x414D32LL), nullptr);
  EXPECT_EQ(map_->Get(0x424F32LL), nullptr);

  EXPECT_EQ(*map_->Get(0x407654LL), "0x407654");
  EXPECT_EQ(*map_->Get(0x33558822DD44AA11LL), "0x33558822DD44AA11");
  EXPECT_EQ(map_->Get(0x00558822DD44AA11LL), nullptr);
  EXPECT_EQ(*map_->Get(0xFFFFFFFFFFFFFFLL), "0xFFFFFFFFFFFFFF");
  EXPECT_EQ(*map_->Get(0xFFFFFFFFFFFFFELL), "0xFFFFFFFFFFFFFE");
  EXPECT_EQ(map_->Get(0xFFFFLL), nullptr);
  EXPECT_EQ(map_->Get(0xFFLL), nullptr);
  EXPECT_EQ(*map_->Get(0x112233445566LL), "0x112233445566");
  EXPECT_EQ(*map_->Get(0x1122AAEE5566LL), "0x1122AAEE5566");
  EXPECT_EQ(map_->Get(0x112333445566LL), nullptr);
  EXPECT_EQ(map_->Get(0x112255445566LL), nullptr);
  EXPECT_EQ(map_->Get(0x112233EE5566LL), nullptr);
  EXPECT_EQ(map_->Get(0x1122AA445566LL), nullptr);

  Remove(0x414F32LL);
  Remove(0x414E00LL);
  Remove(0x407654LL);
  Remove(2);
}

TEST_F(LongAdaptiveRadixTreeMapTest, ShouldExtendTo16andReduceTo4) {
  Put(2, "2");
  Put(223, "223");
  Put(49, "49");
  Put(1, "1");
  // 4->16
  Put(77, "77");
  Put(4, "4");

  Remove(223);
  Remove(1);
  // 16->4
  Remove(4);
  Remove(49);

  // reduce intermediate
  Put(65536LL * 7, "65536*7");
  Put(65536LL * 3, "65536*3");
  Put(65536LL * 2, "65536*2");
  // 4->16
  Put(65536LL * 4, "65536*4");
  Put(65536LL * 3 + 3, "65536*3+3");

  Remove(65536LL * 2);
  // 16->4
  Remove(65536LL * 4);
  Remove(65536LL * 7);
}

TEST_F(LongAdaptiveRadixTreeMapTest, ShouldExtendTo48andReduceTo16) {
  // reduce at end level
  for (int i = 0; i < 16; i++) {
    Put(i, std::to_string(i));
  }
  // 16->48
  Put(177, "177");
  Put(56, "56");
  Put(255, "255");

  Remove(0);
  Remove(16);
  Remove(13);
  Remove(17); // nothing
  Remove(3);
  Remove(5);
  Remove(255);
  Remove(7);
  // 48->16
  Remove(8);
  Remove(2);
  Remove(38);
  Put(4, "4A");

  // reduce intermediate
  for (int i = 0; i < 16; i++) {
    Put(256LL * i, std::to_string(256 * i));
  }

  // 16->48
  Put(256LL * 47, std::to_string(256 * 47));
  Put(256LL * 27, std::to_string(256 * 27));
  Put(256LL * 255, std::to_string(256 * 255));
  Put(256LL * 22, std::to_string(256 * 22));

  Remove(256LL * 5);
  Remove(256LL * 6);
  Remove(256LL * 7);
  Remove(256LL * 8);
  Remove(256LL * 9);
  Remove(256LL * 10);
  Remove(256LL * 11);
  // 48->16
  Remove(256LL * 15);
  Remove(256LL * 13);
  Remove(256LL * 14);
  Remove(256LL * 12);
}

TEST_F(LongAdaptiveRadixTreeMapTest, ShouldExtendTo256andReduceTo48) {
  // reduce at end level
  for (int i = 0; i < 48; i++) {
    int key = 255 - i * 3;
    Put(key, std::to_string(key));
  }

  // 48->256
  Put(176, "176");
  Put(221, "221");

  Remove(252);
  Remove(132);
  Remove(135);
  Remove(138);
  Remove(141);
  Remove(144);
  Remove(147);
  Remove(150);
  Remove(153);
  Remove(156);
  Remove(159);
  Remove(162);
  Remove(165);

  for (int i = 0; i < 50; i++) {
    int64_t key = 65536LL * (13 + i * 3);
    Put(key, std::to_string(key));
  }

  for (int i = 10; i < 30; i++) {
    int64_t key = 65536LL * (13 + i * 3);
    Remove(key);
  }
}

TEST_F(LongAdaptiveRadixTreeMapTest, ShouldLoadManyItems) {
  std::mt19937 rand(1);
  std::uniform_int_distribution<int> dist(1, 1000);

  const int forEachSize = 5000;

  TestConsumerInt64 forEachConsumerArt;
  std::vector<int64_t> forEachKeysBst;
  std::vector<int64_t> forEachValuesBst;

  LongAdaptiveRadixTreeMap<int64_t> art;
  std::map<int64_t, int64_t *> bst;

  int num = 100000;
  std::vector<int64_t> list;
  list.reserve(num);

  int64_t j = 0;
  int64_t offset = 1000000000LL + rand() % 1000000;
  for (int i = 0; i < num; i++) {
    list.push_back(offset + j);
    j += dist(rand);
  }

  // Shuffle
  std::shuffle(list.begin(), list.end(), rand);

  // Put into BST
  for (int64_t x : list) {
    bst[x] = new int64_t(x);
  }

  // Put into ART
  for (int64_t x : list) {
    art.Put(x, new int64_t(x));
  }

  // Shuffle again
  std::shuffle(list.begin(), list.end(), rand);

  // Get (hit) from BST
  int64_t sum = 0;
  for (int64_t x : list) {
    sum += *bst[x];
  }

  // Get (hit) from ART
  for (int64_t x : list) {
    int64_t *result = art.Get(x);
    ASSERT_NE(result, nullptr);
    sum += *result;
  }

  // Validate
  art.ValidateInternalState();

  // Check entries manually (first validation)
  {
    auto artEntries = art.EntriesList();
    auto artIt = artEntries.begin();
    auto bstIt = bst.begin();
    while (artIt != artEntries.end() && bstIt != bst.end()) {
      EXPECT_EQ(artIt->first, bstIt->first);
      EXPECT_EQ(*artIt->second, *bstIt->second);
      ++artIt;
      ++bstIt;
    }
    EXPECT_EQ(artIt, artEntries.end());
    EXPECT_EQ(bstIt, bst.end());
  }

  // Shuffle again
  std::shuffle(list.begin(), list.end(), rand);

  // Validate getHigherValue
  for (int64_t x : list) {
    int64_t *v1 = art.GetHigherValue(x);
    auto it = bst.upper_bound(x);
    int64_t *v2 = (it != bst.end()) ? it->second : nullptr;

    if (v1 == nullptr && v2 == nullptr) {
      continue;
    }
    if (v1 == nullptr || v2 == nullptr) {
      FAIL() << "Mismatch: ART=" << (v1 ? *v1 : -1)
             << ", BST=" << (v2 ? *v2 : -1) << " for key " << x;
    }
    if (v1 != nullptr && v2 != nullptr) {
      EXPECT_EQ(*v1, *v2) << "getHigherValue mismatch for key " << x;
    }
  }

  // Validate getLowerValue
  for (int64_t x : list) {
    int64_t *v1 = art.GetLowerValue(x);
    auto it = bst.lower_bound(x);
    if (it != bst.begin()) {
      --it;
    } else {
      it = bst.end();
    }
    int64_t *v2 = (it != bst.end()) ? it->second : nullptr;

    if (v1 == nullptr && v2 == nullptr) {
      continue;
    }
    if (v1 == nullptr || v2 == nullptr) {
      FAIL() << "Mismatch: ART=" << (v1 ? *v1 : -1)
             << ", BST=" << (v2 ? *v2 : -1) << " for key " << x;
    }
    if (v1 != nullptr && v2 != nullptr) {
      EXPECT_EQ(*v1, *v2) << "getLowerValue mismatch for key " << x;
    }
  }

  // forEach
  forEachConsumerArt.Clear();
  art.ForEach(&forEachConsumerArt, forEachSize);

  forEachKeysBst.clear();
  forEachValuesBst.clear();
  int count = 0;
  for (const auto &entry : bst) {
    if (count++ >= forEachSize)
      break;
    forEachKeysBst.push_back(entry.first);
    forEachValuesBst.push_back(*entry.second);
  }

  ASSERT_EQ(forEachConsumerArt.keys.size(), forEachKeysBst.size());
  for (size_t i = 0; i < forEachKeysBst.size(); i++) {
    EXPECT_EQ(forEachConsumerArt.keys[i], forEachKeysBst[i]);
    EXPECT_EQ(*forEachConsumerArt.values[i], forEachValuesBst[i]);
  }

  forEachConsumerArt.Clear();

  // forEachDesc
  forEachConsumerArt.Clear();
  art.ForEachDesc(&forEachConsumerArt, forEachSize);

  forEachKeysBst.clear();
  forEachValuesBst.clear();
  count = 0;
  for (auto it = bst.rbegin(); it != bst.rend() && count < forEachSize;
       ++it, ++count) {
    forEachKeysBst.push_back(it->first);
    forEachValuesBst.push_back(*it->second);
  }

  ASSERT_EQ(forEachConsumerArt.keys.size(), forEachKeysBst.size());
  for (size_t i = 0; i < forEachKeysBst.size(); i++) {
    EXPECT_EQ(forEachConsumerArt.keys[i], forEachKeysBst[i]);
    EXPECT_EQ(*forEachConsumerArt.values[i], forEachValuesBst[i]);
  }

  forEachConsumerArt.Clear();

  // Remove from BST
  for (int64_t x : list) {
    auto it = bst.find(x);
    if (it != bst.end()) {
      delete it->second;
      bst.erase(it);
    }
  }

  // Remove from ART
  for (int64_t x : list) {
    art.Remove(x);
  }

  // Validate
  art.ValidateInternalState();

  // Check entries manually
  auto artEntries = art.EntriesList();
  auto artIt = artEntries.begin();
  auto bstIt = bst.begin();
  while (artIt != artEntries.end() && bstIt != bst.end()) {
    EXPECT_EQ(artIt->first, bstIt->first);
    EXPECT_EQ(*artIt->second, *bstIt->second);
    ++artIt;
    ++bstIt;
  }
  EXPECT_EQ(artIt, artEntries.end());
  EXPECT_EQ(bstIt, bst.end());

  // Clean up ART values
  for (auto &entry : artEntries) {
    delete entry.second;
  }
}
