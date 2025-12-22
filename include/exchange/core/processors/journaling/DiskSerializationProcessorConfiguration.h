/*
 * Copyright 2019 Maksim Zheravin
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
  static constexpr const char *DEFAULT_FOLDER = "exchange-state";

  std::string storageFolder;
  int32_t journalBufferFlushTrigger;
  int64_t journalFileMaxSize;
  int32_t journalBatchCompressThreshold;

  DiskSerializationProcessorConfiguration(
      const std::string &storageFolder = DEFAULT_FOLDER,
      int32_t journalBufferFlushTrigger = 64 * 1024,
      int64_t journalFileMaxSize = 1024 * 1024 * 1024,
      int32_t journalBatchCompressThreshold = 4096)
      : storageFolder(storageFolder),
        journalBufferFlushTrigger(journalBufferFlushTrigger),
        journalFileMaxSize(journalFileMaxSize),
        journalBatchCompressThreshold(journalBatchCompressThreshold) {}
};

} // namespace journaling
} // namespace processors
} // namespace core
} // namespace exchange
