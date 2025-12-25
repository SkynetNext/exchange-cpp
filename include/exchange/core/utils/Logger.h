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

#include <memory>
#include <mutex>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>
#include <string>
#include <unordered_map>

namespace exchange {
namespace core {
namespace processors {

/**
 * Logger utility - matches Java version's SLF4J + Logback behavior
 * - Synchronous mode (like Logback ConsoleAppender)
 * - Output to stdout (for cloud environments)
 * - Thread-safe (built into spdlog)
 * - Format matches Java logback pattern
 */
inline std::shared_ptr<spdlog::logger>
get_logger(const std::string &name = "exchange_core") {
  static std::mutex init_mutex;
  static std::unordered_map<std::string, std::shared_ptr<spdlog::logger>>
      loggers;

  std::lock_guard<std::mutex> lock(init_mutex);
  auto it = loggers.find(name);
  if (it != loggers.end()) {
    return it->second;
  }

  // Create synchronous stdout logger (matches Logback ConsoleAppender)
  // stdout_color_mt = multi-threaded, synchronous, stdout with colors
  auto logger = spdlog::stdout_color_mt(name);

  // Set format to match Java logback pattern:
  // Java: %d{yyyy-MM-dd/HH:mm:ss.SSS/zzz} [%thread] %-5level %logger{36} -
  // %msg%n C++:  [YYYY-MM-DD HH:MM:SS.mmm] [thread_id] [level] [logger] -
  // message
  logger->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%t] [%^%l%$] [%n] %v");

  // Set log level (matches Java: exchange.core2 level=DEBUG)
  logger->set_level(spdlog::level::debug);

  // Flush on warn and above (ensure important logs are not lost)
  logger->flush_on(spdlog::level::warn);

  loggers[name] = logger;
  return logger;
}

// Convenience macros (similar to Java's log.info(), log.debug(), etc.)
#define LOG_DEBUG(...)                                                         \
  exchange::core::processors::get_logger()->debug(__VA_ARGS__)
#define LOG_INFO(...)                                                          \
  exchange::core::processors::get_logger()->info(__VA_ARGS__)
#define LOG_WARN(...)                                                          \
  exchange::core::processors::get_logger()->warn(__VA_ARGS__)
#define LOG_ERROR(...)                                                         \
  exchange::core::processors::get_logger()->error(__VA_ARGS__)

} // namespace processors
} // namespace core
} // namespace exchange
