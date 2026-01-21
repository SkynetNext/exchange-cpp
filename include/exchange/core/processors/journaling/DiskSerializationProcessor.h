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
#include <fstream>
#include <istream>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <vector>
#include "../../common/cmd/OrderCommand.h"
#include "../../common/config/ExchangeConfiguration.h"
#include "../../common/config/InitialStateConfiguration.h"
#include "DiskSerializationProcessorConfiguration.h"
#include "ISerializationProcessor.h"

// Forward declarations
class ExchangeApi;
class IExchangeApi;

namespace exchange {
namespace core {
namespace processors {
namespace journaling {

// Forward declarations
struct JournalDescriptor;
struct SnapshotDescriptor;

/**
 * DiskSerializationProcessor - disk-based serialization processor
 * Handles snapshots and journaling to disk
 */
class DiskSerializationProcessor : public ISerializationProcessor {
public:
  DiskSerializationProcessor(const common::config::ExchangeConfiguration* exchangeConfig,
                             const DiskSerializationProcessorConfiguration* diskConfig);

  bool StoreData(int64_t snapshotId,
                 int64_t seq,
                 int64_t timestampNs,
                 SerializedModuleType type,
                 int32_t instanceId,
                 const common::WriteBytesMarshallable* obj) override;

  void WriteToJournal(common::cmd::OrderCommand* cmd, int64_t dSeq, bool eob) override;

  void EnableJournaling(int64_t afterSeq, IExchangeApi* api) override;

  std::map<int64_t, SnapshotDescriptor*> FindAllSnapshotPoints() override;

  void
  ReplayJournalStep(int64_t snapshotId, int64_t seqFrom, int64_t seqTo, IExchangeApi* api) override;

  int64_t
  ReplayJournalFull(const common::config::InitialStateConfiguration* initialStateConfiguration,
                    IExchangeApi* api) override;

  void ReplayJournalFullAndThenEnableJouraling(
    const common::config::InitialStateConfiguration* initialStateConfiguration,
    IExchangeApi* api) override;

  bool
  CheckSnapshotExists(int64_t snapshotId, SerializedModuleType type, int32_t instanceId) override;

private:
  std::string exchangeId_;
  std::string folder_;
  int64_t baseSeq_;

  int32_t journalBufferFlushTrigger_;
  int64_t journalFileMaxSize_;
  int32_t journalBatchCompressThreshold_;

  std::map<int64_t, SnapshotDescriptor*> snapshotsIndex_;
  SnapshotDescriptor* lastSnapshotDescriptor_;
  JournalDescriptor* lastJournalDescriptor_;

  int64_t baseSnapshotId_;
  int64_t enableJournalAfterSeq_;

  int32_t filesCounter_;
  int64_t writtenBytes_;
  int64_t lastWrittenSeq_;  // Track last written sequence number for seqLast

  // Journal writing buffers
  std::vector<char> journalWriteBuffer_;
  std::vector<char> lz4WriteBuffer_;
  size_t journalWriteBufferPos_;

  // File handles for journal writing
  std::unique_ptr<std::fstream> journalFile_;
  std::mutex journalMutex_;

  // Internal methods
  std::string GetSnapshotPath(int64_t snapshotId, SerializedModuleType type, int32_t instanceId);
  std::string GetJournalPath(int64_t snapshotId, int32_t fileIndex);

  // Journal writing helpers
  void FlushBufferSync(bool forceStartNextFile, int64_t timestampNs);
  void StartNewFile(int64_t timestampNs);
  void RegisterNextJournal(int64_t seq, int64_t timestampNs);
  void RegisterNextSnapshot(int64_t snapshotId, int64_t seq, int64_t timestampNs);

  // Journal replay helpers
  void
  ReadCommands(std::istream& is, IExchangeApi* api, int64_t& lastSeq, bool insideCompressedBlock);

  // Override LoadData from base class
  void LoadData(int64_t snapshotId,
                SerializedModuleType type,
                int32_t instanceId,
                std::function<void(common::BytesIn*)> initFunc) override;
};

}  // namespace journaling
}  // namespace processors
}  // namespace core
}  // namespace exchange
