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

#include "../../common/BytesIn.h"
#include "../../common/cmd/OrderCommand.h"
#include "../../common/config/InitialStateConfiguration.h"
#include <cstdint>
#include <functional>
#include <map>

// Forward declarations
class ExchangeApi;

namespace exchange {
namespace core {
namespace processors {
namespace journaling {

// Forward declarations
struct SnapshotDescriptor;

/**
 * ISerializationProcessor - interface for serialization/deserialization
 */
class ISerializationProcessor {
public:
  enum class SerializedModuleType { RISK_ENGINE, MATCHING_ENGINE_ROUTER };

  virtual ~ISerializationProcessor() = default;

  /**
   * Serialize state into storage
   */
  virtual bool StoreData(int64_t snapshotId, int64_t seq, int64_t timestampNs,
                         SerializedModuleType type, int32_t instanceId,
                         void *obj) = 0;

  /**
   * Deserialize state from storage
   */
  template <typename T>
  T LoadData(int64_t snapshotId, SerializedModuleType type, int32_t instanceId,
             std::function<T(common::BytesIn *)> initFunc) {
    // Call virtual method LoadDataVoid (type-erased)
    return LoadDataVoid(snapshotId, type, instanceId,
                        [&initFunc](common::BytesIn *bytesIn) -> void * {
                          if constexpr (std::is_same_v<T, void *>) {
                            return initFunc(bytesIn);
                          } else {
                            // For non-void* types, we need to store the result
                            // This is a limitation of type erasure
                            static thread_local T result;
                            result = initFunc(bytesIn);
                            return &result;
                          }
                        });
  }

  /**
   * Write command into journal
   */
  virtual void WriteToJournal(common::cmd::OrderCommand *cmd, int64_t dSeq,
                              bool eob) = 0;

  /**
   * Activate journal
   */
  virtual void EnableJournaling(int64_t afterSeq, void *api) = 0;

  /**
   * Get all available snapshots
   */
  virtual std::map<int64_t, SnapshotDescriptor *> FindAllSnapshotPoints() = 0;

  /**
   * Replay journal step
   */
  virtual void ReplayJournalStep(int64_t snapshotId, int64_t seqFrom,
                                 int64_t seqTo, void *api) = 0;

  /**
   * Replay journal full
   */
  virtual int64_t
  ReplayJournalFull(const common::config::InitialStateConfiguration
                        *initialStateConfiguration,
                    void *api) = 0;

  /**
   * Replay journal full and then enable journaling
   */
  virtual void ReplayJournalFullAndThenEnableJouraling(
      const common::config::InitialStateConfiguration
          *initialStateConfiguration,
      void *api) = 0;

  /**
   * Check if snapshot exists
   */
  virtual bool CheckSnapshotExists(int64_t snapshotId,
                                   SerializedModuleType type,
                                   int32_t instanceId) = 0;

  /**
   * Check if can load from snapshot
   */
  static bool CanLoadFromSnapshot(
      ISerializationProcessor *serializationProcessor,
      const common::config::InitialStateConfiguration *initStateCfg,
      int32_t shardId, SerializedModuleType module);

protected:
  /**
   * Virtual method for loading data (type-erased to void*)
   * Derived classes should override this
   */
  virtual void *
  LoadDataVoid(int64_t snapshotId, SerializedModuleType type,
               int32_t instanceId,
               std::function<void *(common::BytesIn *)> initFunc) {
    // Default implementation: return initFunc(nullptr)
    return initFunc(nullptr);
  }
};

} // namespace journaling
} // namespace processors
} // namespace core
} // namespace exchange
