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

#include <functional>

namespace exchange {
namespace core {
// Forward declaration for ISerializationProcessor (outside config namespace)
namespace processors {
namespace journaling {
class ISerializationProcessor;
}
} // namespace processors
namespace common {
namespace config {

// Forward declarations
class ExchangeConfiguration;

/**
 * SerializationConfiguration - serialization (snapshots and journaling)
 * configuration
 * Match Java: SerializationConfiguration with factory pattern
 */
class SerializationConfiguration {
public:
  bool enableJournaling;
  // Use fully qualified namespace to avoid ambiguity in config namespace
  std::function<
      ::exchange::core::processors::journaling::ISerializationProcessor *(
          const ExchangeConfiguration *)>
      serializationProcessorFactory;

  SerializationConfiguration(
      bool enableJournaling,
      std::function<
          ::exchange::core::processors::journaling::ISerializationProcessor *(
              const ExchangeConfiguration *)>
          factory)
      : enableJournaling(enableJournaling),
        serializationProcessorFactory(factory) {}

  static SerializationConfiguration Default();
  static SerializationConfiguration DiskSnapshotOnly();
  static SerializationConfiguration DiskJournaling();
};

} // namespace config
} // namespace common
} // namespace core
} // namespace exchange
