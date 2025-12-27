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

namespace exchange {
namespace core {
namespace common {
namespace config {

// Forward declaration
class ExchangeConfiguration;

/**
 * SerializationConfiguration - serialization (snapshots and journaling)
 * configuration Simplified version for now
 */
class SerializationConfiguration {
public:
  bool enableJournaling;

  SerializationConfiguration(bool enableJournaling)
      : enableJournaling(enableJournaling) {}

  static SerializationConfiguration Default() {
    return SerializationConfiguration(false);
  }

  static SerializationConfiguration DiskSnapshotOnly() {
    return SerializationConfiguration(false);
  }

  static SerializationConfiguration DiskJournaling() {
    return SerializationConfiguration(true);
  }
};

} // namespace config
} // namespace common
} // namespace core
} // namespace exchange
