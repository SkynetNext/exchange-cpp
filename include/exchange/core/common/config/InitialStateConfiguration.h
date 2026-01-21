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
#include <limits>
#include <string>

namespace exchange::core::common::config {

/**
 * InitialStateConfiguration - exchange initialization configuration
 */
class InitialStateConfiguration {
public:
  std::string exchangeId;
  int64_t snapshotId;
  int64_t snapshotBaseSeq;
  int64_t journalTimestampNs;
  bool throwIfSnapshotNotFound;

  InitialStateConfiguration(std::string exchangeId,
                            int64_t snapshotId,
                            int64_t snapshotBaseSeq,
                            int64_t journalTimestampNs,
                            bool throwIfSnapshotNotFound)
    : exchangeId(std::move(exchangeId))
    , snapshotId(snapshotId)
    , snapshotBaseSeq(snapshotBaseSeq)
    , journalTimestampNs(journalTimestampNs)
    , throwIfSnapshotNotFound(throwIfSnapshotNotFound) {}

  bool FromSnapshot() const {
    return snapshotId != 0;
  }

  static InitialStateConfiguration CleanStart(const std::string& exchangeId) {
    return InitialStateConfiguration(exchangeId, 0, 0, 0, false);
  }

  static InitialStateConfiguration Default() {
    return CleanStart("MY_EXCHANGE");
  }

  static InitialStateConfiguration CleanTest() {
    return CleanStart("EC0");
  }

  /**
   * Clean start configuration with journaling on.
   * @param exchangeId Exchange ID
   * @return clean start configuration with journaling on.
   */
  static InitialStateConfiguration CleanStartJournaling(const std::string& exchangeId) {
    return InitialStateConfiguration(exchangeId, 0, 0, 0, true);
  }

  /**
   * Configuration that loads from snapshot, without journal replay with
   * journaling off.
   * @param exchangeId Exchange ID
   * @param snapshotId snapshot ID
   * @param baseSeq base seq
   * @return configuration that loads from snapshot, without journal replay with
   * journaling off.
   */
  static InitialStateConfiguration
  FromSnapshotOnly(const std::string& exchangeId, int64_t snapshotId, int64_t baseSeq) {
    return InitialStateConfiguration(exchangeId, snapshotId, baseSeq, 0, true);
  }

  /**
   * Configuration that load exchange from last known state including journal
   * replay till last known start. Journal is enabled.
   * @param exchangeId Exchange ID
   * @param snapshotId snapshot ID
   * @param baseSeq base seq
   * @return configuration that load exchange from last known state including
   * journal replay till last known start. Journal is enabled.
   */
  static InitialStateConfiguration
  LastKnownStateFromJournal(const std::string& exchangeId, int64_t snapshotId, int64_t baseSeq) {
    return InitialStateConfiguration(exchangeId, snapshotId, baseSeq,
                                     std::numeric_limits<int64_t>::max(), true);
  }
};

}  // namespace exchange::core::common::config
