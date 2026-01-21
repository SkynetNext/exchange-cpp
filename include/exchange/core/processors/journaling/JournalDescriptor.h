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

namespace exchange::core::processors::journaling {

// Forward declaration
struct SnapshotDescriptor;

/**
 * JournalDescriptor - describes a journal file
 * Matches Java: JournalDescriptor with linked list structure
 */
struct JournalDescriptor {
  int64_t timestampNs;
  int64_t seqFirst;
  int64_t seqLast = -1;  // -1 if not finished yet

  SnapshotDescriptor* baseSnapshot;

  // Linked list structure
  JournalDescriptor* prev;            // can be null
  JournalDescriptor* next = nullptr;  // can be null

  JournalDescriptor(int64_t timestampNs,
                    int64_t seqFirst,
                    SnapshotDescriptor* baseSnapshot,
                    JournalDescriptor* prev)
    : timestampNs(timestampNs), seqFirst(seqFirst), baseSnapshot(baseSnapshot), prev(prev) {
    if (prev) {
      prev->next = this;
    }
  }
};

}  // namespace exchange::core::processors::journaling
