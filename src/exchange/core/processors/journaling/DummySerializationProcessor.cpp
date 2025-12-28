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

#include <exchange/core/common/cmd/OrderCommand.h>
#include <exchange/core/common/config/InitialStateConfiguration.h>
#include <exchange/core/processors/journaling/DummySerializationProcessor.h>
#include <map>

namespace exchange {
namespace core {
namespace processors {
namespace journaling {

DummySerializationProcessor *DummySerializationProcessor::Instance() {
  static DummySerializationProcessor instance;
  return &instance;
}

bool DummySerializationProcessor::StoreData(
    int64_t snapshotId, int64_t seq, int64_t timestampNs,
    SerializedModuleType type, int32_t instanceId,
    const common::WriteBytesMarshallable *obj) {
  // Dummy implementation - do nothing
  return false;
}

void DummySerializationProcessor::LoadData(
    int64_t snapshotId, SerializedModuleType type, int32_t instanceId,
    std::function<void(common::BytesIn *)> initFunc) {
  // Dummy implementation - do nothing
}

void DummySerializationProcessor::WriteToJournal(common::cmd::OrderCommand *cmd,
                                                 int64_t dSeq, bool eob) {
  // Dummy implementation - do nothing
}

void DummySerializationProcessor::EnableJournaling(int64_t afterSeq,
                                                   IExchangeApi *api) {
  // Dummy implementation - do nothing
}

std::map<int64_t, SnapshotDescriptor *>
DummySerializationProcessor::FindAllSnapshotPoints() {
  // Dummy implementation - return empty map
  return std::map<int64_t, SnapshotDescriptor *>();
}

void DummySerializationProcessor::ReplayJournalStep(int64_t snapshotId,
                                                    int64_t seqFrom,
                                                    int64_t seqTo, IExchangeApi *api) {
  // Dummy implementation - do nothing
}

int64_t DummySerializationProcessor::ReplayJournalFull(
    const common::config::InitialStateConfiguration *initialStateConfiguration,
    IExchangeApi *api) {
  // Dummy implementation - return 0
  return 0;
}

void DummySerializationProcessor::ReplayJournalFullAndThenEnableJouraling(
    const common::config::InitialStateConfiguration *initialStateConfiguration,
    IExchangeApi *api) {
  // Dummy implementation - do nothing
}

bool DummySerializationProcessor::CheckSnapshotExists(int64_t snapshotId,
                                                      SerializedModuleType type,
                                                      int32_t instanceId) {
  // Dummy implementation - return false
  return false;
}

} // namespace journaling
} // namespace processors
} // namespace core
} // namespace exchange
