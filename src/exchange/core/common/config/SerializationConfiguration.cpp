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

#include <exchange/core/common/config/ExchangeConfiguration.h>
#include <exchange/core/common/config/SerializationConfiguration.h>
#include <exchange/core/processors/journaling/DiskSerializationProcessor.h>
#include <exchange/core/processors/journaling/DiskSerializationProcessorConfiguration.h>
#include <exchange/core/processors/journaling/DummySerializationProcessor.h>

namespace exchange::core::common::config {

namespace {
using ::exchange::core::processors::journaling::DiskSerializationProcessor;
using ::exchange::core::processors::journaling::DiskSerializationProcessorConfiguration;
using ::exchange::core::processors::journaling::ISerializationProcessor;
}  // anonymous namespace

SerializationConfiguration SerializationConfiguration::Default() {
  auto factory = [](const ExchangeConfiguration*)
    -> ::exchange::core::processors::journaling::ISerializationProcessor* {
    return ::exchange::core::processors::journaling::DummySerializationProcessor::Instance();
  };
  return SerializationConfiguration(false, factory);
}

SerializationConfiguration SerializationConfiguration::DiskSnapshotOnly() {
  using namespace ::exchange::core::processors::journaling;
  auto factory = [](const ExchangeConfiguration* exchangeCfg) -> ISerializationProcessor* {
    static DiskSerializationProcessorConfiguration defaultConfig;
    // Explicit conversion to base class pointer
    return static_cast<ISerializationProcessor*>(
      new DiskSerializationProcessor(exchangeCfg, &defaultConfig));
  };
  return SerializationConfiguration(false, factory);
}

SerializationConfiguration SerializationConfiguration::DiskJournaling() {
  using namespace ::exchange::core::processors::journaling;
  auto factory = [](const ExchangeConfiguration* exchangeCfg) -> ISerializationProcessor* {
    static DiskSerializationProcessorConfiguration defaultConfig;
    // Explicit conversion to base class pointer
    return static_cast<ISerializationProcessor*>(
      new DiskSerializationProcessor(exchangeCfg, &defaultConfig));
  };
  return SerializationConfiguration(true, factory);
}

}  // namespace exchange::core::common::config
