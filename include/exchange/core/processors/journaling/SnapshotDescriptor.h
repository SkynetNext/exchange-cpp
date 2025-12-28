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
#include <string>

namespace exchange {
namespace core {
namespace processors {
namespace journaling {

// Forward declaration
struct JournalDescriptor;

/**
 * SnapshotDescriptor - describes a snapshot
 * Matches Java: SnapshotDescriptor with linked list structure
 */
struct SnapshotDescriptor {
  int64_t snapshotId; // 0 means empty snapshot (clean start)
  int64_t seq;        // sequence when snapshot was made
  int64_t timestampNs;
  std::string path;

  // Linked list structure
  SnapshotDescriptor *prev;
  SnapshotDescriptor *next;

  int32_t numMatchingEngines;
  int32_t numRiskEngines;

  // All journals based on this snapshot
  // mapping: startingSeq -> JournalDescriptor*
  std::map<int64_t, JournalDescriptor *> journals;

  SnapshotDescriptor(int64_t snapshotId, int64_t seq, int64_t timestampNs,
                     SnapshotDescriptor *prev, int32_t numMatchingEngines,
                     int32_t numRiskEngines)
      : snapshotId(snapshotId), seq(seq), timestampNs(timestampNs), prev(prev),
        next(nullptr), numMatchingEngines(numMatchingEngines),
        numRiskEngines(numRiskEngines) {
    if (prev) {
      prev->next = this;
    }
  }

  /**
   * Create initial empty snapshot descriptor
   */
  static SnapshotDescriptor *CreateEmpty(int32_t initialNumME,
                                         int32_t initialNumRE) {
    return new SnapshotDescriptor(0, 0, 0, nullptr, initialNumME, initialNumRE);
  }

  /**
   * Create next snapshot descriptor
   */
  SnapshotDescriptor *CreateNext(int64_t snapshotId, int64_t seq,
                                 int64_t timestampNs) {
    return new SnapshotDescriptor(snapshotId, seq, timestampNs, this,
                                  numMatchingEngines, numRiskEngines);
  }
};

} // namespace journaling
} // namespace processors
} // namespace core
} // namespace exchange
