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

#pragma once

#include <cstdint>
#include <map>
#include <memory>
#include <vector>
#include "ReportResult.h"

namespace exchange::core::common {
class BytesIn;
class BytesOut;

namespace api::reports {

/**
 * StateHashReportResult - state hash report result
 */
class StateHashReportResult : public ReportResult {
public:
  enum class ModuleType { RISK_ENGINE, MATCHING_ENGINE };

  enum class SubmoduleType {
    RISK_SYMBOL_SPEC_PROVIDER = 0,
    RISK_USER_PROFILE_SERVICE = 1,
    RISK_BINARY_CMD_PROCESSOR = 2,
    RISK_LAST_PRICE_CACHE = 3,
    RISK_FEES = 4,
    RISK_ADJUSTMENTS = 5,
    RISK_SUSPENDS = 6,
    RISK_SHARD_MASK = 7,
    MATCHING_BINARY_CMD_PROCESSOR = 64,
    MATCHING_ORDER_BOOKS = 65,
    MATCHING_SHARD_MASK = 66
  };

  struct SubmoduleKey {
    int32_t moduleId;
    SubmoduleType submodule;

    SubmoduleKey(int32_t moduleId, SubmoduleType submodule)
      : moduleId(moduleId), submodule(submodule) {}

    /**
     * Constructor from BytesIn (deserialization)
     */
    explicit SubmoduleKey(BytesIn& bytes);

    bool operator<(const SubmoduleKey& other) const {
      if (submodule != other.submodule) {
        return static_cast<int32_t>(submodule) < static_cast<int32_t>(other.submodule);
      }
      return moduleId < other.moduleId;
    }

    bool operator==(const SubmoduleKey& other) const {
      return moduleId == other.moduleId && submodule == other.submodule;
    }

    // Serialization method (not inheriting WriteBytesMarshallable)
    void WriteMarshallable(BytesOut& bytes) const;
  };

  static SubmoduleKey CreateKey(int32_t moduleId, SubmoduleType submoduleType) {
    return SubmoduleKey(moduleId, submoduleType);
  }

  std::map<SubmoduleKey, int32_t> hashCodes;

  explicit StateHashReportResult(const std::map<SubmoduleKey, int32_t>& hashCodes = {})
    : hashCodes(hashCodes) {}

  // Constructor from BytesIn (deserialization)
  explicit StateHashReportResult(BytesIn& bytes);

  int32_t GetStateHash() const;

  // Serialization method
  void WriteMarshallable(BytesOut& bytes) const;

  // Merge method (matches Java merge)
  static std::unique_ptr<StateHashReportResult> Merge(const std::vector<BytesIn*>& pieces);

  static const StateHashReportResult EMPTY;
};

}  // namespace api::reports

}  // namespace exchange::core::common
