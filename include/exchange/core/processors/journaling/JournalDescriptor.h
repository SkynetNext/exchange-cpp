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
#include <string>

namespace exchange {
namespace core {
namespace processors {
namespace journaling {

/**
 * JournalDescriptor - describes a journal file
 */
struct JournalDescriptor {
  int64_t snapshotId;
  int64_t seqFrom;
  int64_t seqTo;
  std::string path;

  JournalDescriptor(int64_t snapshotId, int64_t seqFrom, int64_t seqTo,
                    const std::string &path)
      : snapshotId(snapshotId), seqFrom(seqFrom), seqTo(seqTo), path(path) {}
};

} // namespace journaling
} // namespace processors
} // namespace core
} // namespace exchange
