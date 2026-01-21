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

#include <exchange/core/common/BytesIn.h>
#include <exchange/core/common/BytesOut.h>
#include <exchange/core/common/api/reports/StateHashReportResult.h>
#include <exchange/core/utils/SerializationUtils.h>
#include <functional>
#include <vector>

namespace exchange::core::common::api::reports {

StateHashReportResult::SubmoduleKey::SubmoduleKey(BytesIn& bytes) {
    moduleId = bytes.ReadInt();
    int32_t submoduleCode = bytes.ReadInt();
    // Convert code to SubmoduleType enum
    submodule = static_cast<SubmoduleType>(submoduleCode);
}

void StateHashReportResult::SubmoduleKey::WriteMarshallable(BytesOut& bytes) const {
    bytes.WriteInt(moduleId);
    bytes.WriteInt(static_cast<int32_t>(submodule));
}

StateHashReportResult::StateHashReportResult(BytesIn& bytes) {
    // Match Java: SerializationUtils.readGenericMap(bytesIn, TreeMap::new,
    // SubmoduleKey::new, BytesIn::readInt)
    int32_t size = bytes.ReadInt();
    for (int32_t i = 0; i < size; i++) {
        SubmoduleKey key(bytes);
        int32_t value = bytes.ReadInt();
        hashCodes[key] = value;
    }
}

void StateHashReportResult::WriteMarshallable(BytesOut& bytes) const {
    // Match Java: SerializationUtils.marshallGenericMap(hashCodes, bytes, (b, k)
    // -> k.writeMarshallable(b), BytesOut::writeInt) Directly implement to avoid
    // lambda type inference issues
    bytes.WriteInt(static_cast<int32_t>(hashCodes.size()));
    for (const auto& pair : hashCodes) {
        pair.first.WriteMarshallable(bytes);
        bytes.WriteInt(pair.second);
    }
}

std::unique_ptr<StateHashReportResult> StateHashReportResult::Merge(
    const std::vector<BytesIn*>& pieces) {
    // Match Java: merge(final Stream<BytesIn> pieces)
    if (pieces.empty()) {
        return std::make_unique<StateHashReportResult>(EMPTY);
    }

    // Start with first piece
    auto result = std::make_unique<StateHashReportResult>(*pieces[0]);

    // Merge remaining pieces
    for (size_t i = 1; i < pieces.size(); i++) {
        auto next = std::make_unique<StateHashReportResult>(*pieces[i]);

        // Merge logic (matches Java reduce): putAll
        for (const auto& pair : next->hashCodes) {
            result->hashCodes[pair.first] = pair.second;
        }
    }

    return result;
}

int32_t StateHashReportResult::GetStateHash() const {
    // Match Java implementation:
    // final int[] hashes = hashCodes.entrySet().stream()
    //         .mapToInt(e -> Objects.hash(e.getKey(), e.getValue())).toArray();
    // return Arrays.hashCode(hashes);

    // Step 1: Calculate hash for each entry (key, value) pair
    std::vector<int32_t> hashes;
    hashes.reserve(hashCodes.size());
    for (const auto& pair : hashCodes) {
        // Objects.hash(key, value) equivalent:
        // hash = 31 * (31 + key.hashCode()) + value.hashCode()
        // For SubmoduleKey, we use its operator< for hashing
        std::size_t keyHash =
            std::hash<int32_t>{}(pair.first.moduleId)
            ^ (std::hash<int32_t>{}(static_cast<int32_t>(pair.first.submodule)) << 1);
        int32_t entryHash =
            static_cast<int32_t>(31 * (31 + static_cast<int32_t>(keyHash)) + pair.second);
        hashes.push_back(entryHash);
    }

    // Step 2: Arrays.hashCode(hashes) equivalent
    // Arrays.hashCode uses: result = 1; for (int e : a) result = 31 * result + e;
    int32_t result = 1;
    for (int32_t hash : hashes) {
        result = 31 * result + hash;
    }
    return result;
}

// Static EMPTY constant
const StateHashReportResult StateHashReportResult::EMPTY =
    StateHashReportResult(std::map<SubmoduleKey, int32_t>());

}  // namespace exchange::core::common::api::reports
