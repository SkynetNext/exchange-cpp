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

#include <map>
#include "ISerializationProcessor.h"

// Forward declarations
class ExchangeApi;
class IExchangeApi;

namespace exchange {
namespace core {
namespace processors {
namespace journaling {

/**
 * DummySerializationProcessor - dummy implementation that does nothing
 */
class DummySerializationProcessor : public ISerializationProcessor {
public:
  static DummySerializationProcessor* Instance();

  bool StoreData(int64_t snapshotId,
                 int64_t seq,
                 int64_t timestampNs,
                 SerializedModuleType type,
                 int32_t instanceId,
                 const common::WriteBytesMarshallable* obj) override;

  void LoadData(int64_t snapshotId,
                SerializedModuleType type,
                int32_t instanceId,
                std::function<void(common::BytesIn*)> initFunc) override;

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
  DummySerializationProcessor() = default;
};

}  // namespace journaling
}  // namespace processors
}  // namespace core
}  // namespace exchange
