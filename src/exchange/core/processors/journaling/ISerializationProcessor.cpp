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

#include <exchange/core/common/config/InitialStateConfiguration.h>
#include <exchange/core/processors/journaling/ISerializationProcessor.h>

namespace exchange::core::processors::journaling {

bool ISerializationProcessor::CanLoadFromSnapshot(
    ISerializationProcessor* serializationProcessor,
    const common::config::InitialStateConfiguration* initStateCfg,
    int32_t shardId,
    SerializedModuleType module) {
    if (initStateCfg == nullptr || !initStateCfg->FromSnapshot()) {
        return false;
    }

    if (serializationProcessor == nullptr) {
        return false;
    }

    const bool snapshotExists =
        serializationProcessor->CheckSnapshotExists(initStateCfg->snapshotId, module, shardId);

    if (snapshotExists) {
        // snapshot requested and exists
        return true;
    } else if (initStateCfg->throwIfSnapshotNotFound) {
        // snapshot requested but not found, and throw flag is set
        throw std::runtime_error("Snapshot not found: " + std::to_string(initStateCfg->snapshotId));
    } else {
        // snapshot requested but not found, and throw flag is not set
        return false;
    }
}

}  // namespace exchange::core::processors::journaling
