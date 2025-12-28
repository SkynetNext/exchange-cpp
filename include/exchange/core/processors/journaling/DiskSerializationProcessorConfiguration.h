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
 * DiskSerializationProcessorConfiguration - configuration for disk
 * serialization
 */
class DiskSerializationProcessorConfiguration {
public:
  static constexpr const char *DEFAULT_FOLDER = "./dumps";

  std::string storageFolder;
  int32_t journalBufferSize; // Buffer size for journal writing
  int32_t journalBufferFlushTrigger;
  int64_t journalFileMaxSize;
  int32_t journalBatchCompressThreshold;

  DiskSerializationProcessorConfiguration(
      const std::string &storageFolder = DEFAULT_FOLDER,
      int32_t journalBufferSize = 256 * 1024, // 256 KB default
      int64_t journalFileMaxSize = 1024 * 1024 * 1024,
      int32_t journalBatchCompressThreshold = 4096)
      : storageFolder(storageFolder), journalBufferSize(journalBufferSize),
        journalBufferFlushTrigger(journalBufferSize -
                                  256), // MAX_COMMAND_SIZE_BYTES
        journalFileMaxSize(journalFileMaxSize - journalBufferSize),
        journalBatchCompressThreshold(journalBatchCompressThreshold) {}
};

} // namespace journaling
} // namespace processors
} // namespace core
} // namespace exchange
