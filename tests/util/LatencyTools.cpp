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

#include "LatencyTools.h"
#include <cmath>
#include <sstream>

namespace exchange {
namespace core {
namespace tests {
namespace util {

std::string LatencyTools::FormatNanos(int64_t ns) {
  float value = ns / 1000.0f;
  std::string timeUnit = "µs";

  if (value > 1000) {
    value /= 1000;
    timeUnit = "ms";
  }

  if (value > 1000) {
    value /= 1000;
    timeUnit = "s";
  }

  std::ostringstream oss;
  if (value < 3) {
    oss << std::round(value * 100) / 100.0f << timeUnit;
  } else if (value < 30) {
    oss << std::round(value * 10) / 10.0f << timeUnit;
  } else {
    oss << std::round(value) << timeUnit;
  }

  return oss.str();
}

}  // namespace util
}  // namespace tests
}  // namespace core
}  // namespace exchange
