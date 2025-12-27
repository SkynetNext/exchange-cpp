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

#include "ITFeesExchangeLatency.h"
#include <exchange/core/common/config/PerformanceConfiguration.h>

namespace exchange {
namespace core {
namespace tests {
namespace integration {

ITFeesExchangeLatency::ITFeesExchangeLatency() : ITFeesExchange() {}

exchange::core::common::config::PerformanceConfiguration
ITFeesExchangeLatency::GetPerformanceConfiguration() {
  // Note: latencyPerformanceBuilder() equivalent - use Default() with
  // latency-optimized settings
  auto cfg =
      exchange::core::common::config::PerformanceConfiguration::Default();
  // Latency-optimized settings would be applied here if needed
  return cfg;
}

} // namespace integration
} // namespace tests
} // namespace core
} // namespace exchange
