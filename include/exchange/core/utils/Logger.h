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

#include <cstdlib>
#include <memory>
#include <mutex>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>
#include <string>
#include <unordered_map>
#ifdef _WIN32
#include <stdlib.h> // For _dupenv_s on Windows
#endif

namespace exchange {
namespace core {
namespace processors {

/**
 * High-performance Logger utility for HFT systems
 *
 * Optimizations:
 * 1. Cached default logger (avoid lookup on every call)
 * 2. Level check before argument construction (zero overhead when disabled)
 * 3. std::call_once for initialization (faster after first call)
 * 4. Runtime log level configuration via EXCHANGE_LOG_LEVEL env var
 *
 * Matches Java version's SLF4J + Logback behavior:
 * - Synchronous mode (like Logback ConsoleAppender)
 * - Output to stdout (for cloud environments)
 * - Thread-safe (built into spdlog)
 * - Format matches Java logback pattern
 */
inline std::shared_ptr<spdlog::logger>
get_logger(const std::string &name = "exchange_core") {
  static std::mutex registry_mutex;
  static std::unordered_map<std::string, std::shared_ptr<spdlog::logger>>
      loggers;

  // Fast path: check with lock (safe and simple)
  // For HFT: mutex contention is minimal since loggers are created once
  {
    std::lock_guard<std::mutex> lock(registry_mutex);
    auto it = loggers.find(name);
    if (it != loggers.end()) {
      return it->second;
    }
  }

  // Slow path: create logger (only once per name)
  std::lock_guard<std::mutex> lock(registry_mutex);
  // Double-check after acquiring lock (another thread might have created it)
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

  // Runtime log level configuration (for production tuning)
  // Set EXCHANGE_LOG_LEVEL env var: trace, debug, info, warn, error, off
  std::string level_str;
#ifdef _WIN32
  // Windows: use _dupenv_s to avoid deprecation warning
  char *buffer = nullptr;
  size_t size = 0;
  if (_dupenv_s(&buffer, &size, "EXCHANGE_LOG_LEVEL") == 0 &&
      buffer != nullptr) {
    level_str = buffer;
    free(buffer); // Free buffer allocated by _dupenv_s
  }
#else
  // POSIX: use std::getenv (safe on non-Windows platforms)
  const char *log_level_env = std::getenv("EXCHANGE_LOG_LEVEL");
  if (log_level_env != nullptr) {
    level_str = log_level_env;
  }
#endif
  if (!level_str.empty()) {
    if (level_str == "trace") {
      logger->set_level(spdlog::level::trace);
    } else if (level_str == "debug") {
      logger->set_level(spdlog::level::debug);
    } else if (level_str == "info") {
      logger->set_level(spdlog::level::info);
    } else if (level_str == "warn") {
      logger->set_level(spdlog::level::warn);
    } else if (level_str == "error") {
      logger->set_level(spdlog::level::err);
    } else if (level_str == "off") {
      logger->set_level(spdlog::level::off);
    } else {
      logger->set_level(spdlog::level::debug); // default
    }
  } else {
    // Default: debug level (matches Java: exchange.core2 level=DEBUG)
    logger->set_level(spdlog::level::debug);
  }

  // Flush on warn and above (ensure important logs are not lost)
  logger->flush_on(spdlog::level::warn);

  loggers[name] = logger;
  return logger;
}

// Cached default logger for performance (avoid lookup on every call)
// This is safe because logger is created once and never destroyed
inline spdlog::logger *get_default_logger() {
  static std::shared_ptr<spdlog::logger> default_logger =
      get_logger("exchange_core");
  return default_logger.get();
}

// High-performance macros with level checking
// Zero overhead when log level is disabled (arguments are not constructed)
#define LOG_DEBUG(...)                                                         \
  do {                                                                         \
    auto *logger = exchange::core::processors::get_default_logger();           \
    if (logger->should_log(spdlog::level::debug)) {                            \
      logger->debug(__VA_ARGS__);                                              \
    }                                                                          \
  } while (0)

#define LOG_INFO(...)                                                          \
  do {                                                                         \
    auto *logger = exchange::core::processors::get_default_logger();           \
    if (logger->should_log(spdlog::level::info)) {                             \
      logger->info(__VA_ARGS__);                                               \
    }                                                                          \
  } while (0)

#define LOG_WARN(...)                                                          \
  do {                                                                         \
    auto *logger = exchange::core::processors::get_default_logger();           \
    if (logger->should_log(spdlog::level::warn)) {                             \
      logger->warn(__VA_ARGS__);                                               \
    }                                                                          \
  } while (0)

#define LOG_ERROR(...)                                                         \
  do {                                                                         \
    auto *logger = exchange::core::processors::get_default_logger();           \
    if (logger->should_log(spdlog::level::err)) {                              \
      logger->error(__VA_ARGS__);                                              \
    }                                                                          \
  } while (0)

} // namespace processors
} // namespace core
} // namespace exchange
