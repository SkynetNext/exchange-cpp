/*
 * Copyright 2019 Maksim Zheravin
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

#include <set>

namespace exchange {
namespace core {
namespace common {
namespace config {

/**
 * LoggingConfiguration - logging configuration
 */
class LoggingConfiguration {
public:
  enum class LoggingLevel {
    LOGGING_WARNINGS,
    LOGGING_RISK_DEBUG,
    LOGGING_MATCHING_DEBUG
  };

  std::set<LoggingLevel> loggingLevels;

  LoggingConfiguration(std::set<LoggingLevel> loggingLevels)
      : loggingLevels(std::move(loggingLevels)) {}

  static LoggingConfiguration Default() {
    return LoggingConfiguration({LoggingLevel::LOGGING_WARNINGS});
  }

  bool Contains(LoggingLevel level) const {
    return loggingLevels.find(level) != loggingLevels.end();
  }
};

} // namespace config
} // namespace common
} // namespace core
} // namespace exchange
