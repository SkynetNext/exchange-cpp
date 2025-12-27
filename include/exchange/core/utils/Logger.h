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

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdlib>
#include <iostream>
#include <iterator>
#include <memory>
#include <mutex>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>
#include <stdexcept>
#include <string>
#include <string_view>
#ifdef _WIN32
#include <stdlib.h> // For _dupenv_s on Windows
#endif

namespace exchange {
namespace core {
namespace utils {

// Convert string to lowercase (case-insensitive env var parsing)
inline std::string to_lower(std::string_view str) {
  std::string result;
  result.reserve(str.size());
  std::transform(str.begin(), str.end(), std::back_inserter(result),
                 [](unsigned char c) { return std::tolower(c); });
  return result;
}

// Log level mapping for parsing
struct LogLevelMapping {
  std::string_view name;
  spdlog::level::level_enum level;
};

inline constexpr std::array<LogLevelMapping, 6> LOG_LEVEL_MAP = {{
    {"trace", spdlog::level::trace},
    {"debug", spdlog::level::debug},
    {"info", spdlog::level::info},
    {"warn", spdlog::level::warn},
    {"error", spdlog::level::err},
    {"off", spdlog::level::off},
}};

// Parse log level from lowercase string
inline spdlog::level::level_enum
parse_log_level_impl(std::string_view level_lower) {
  for (const auto &mapping : LOG_LEVEL_MAP) {
    if (level_lower == mapping.name) {
      return mapping.level;
    }
  }
  return spdlog::level::debug;
}

// Parse log level from string (case-insensitive)
inline spdlog::level::level_enum parse_log_level(std::string_view level_str) {
  if (level_str.empty()) {
    return spdlog::level::debug;
  }

  std::string level_lower = to_lower(level_str);
  return parse_log_level_impl(level_lower);
}

// Get log level from LOG_LEVEL env var (cached, thread-safe)
inline spdlog::level::level_enum get_cached_log_level() {
  static std::once_flag init_flag;
  static spdlog::level::level_enum cached_level = spdlog::level::debug;

  std::call_once(init_flag, []() {
    std::string level_str;
#ifdef _WIN32
    char *buffer = nullptr;
    size_t size = 0;
    if (_dupenv_s(&buffer, &size, "LOG_LEVEL") == 0 && buffer != nullptr) {
      level_str = buffer;
      free(buffer);
    }
#else
    const char *log_level_env = std::getenv("LOG_LEVEL");
    if (log_level_env != nullptr) {
      level_str = log_level_env;
    }
#endif
    cached_level = parse_log_level(level_str);
  });

  return cached_level;
}

// Create and configure the global logger
inline std::shared_ptr<spdlog::logger> create_configured_logger() {
  try {
    constexpr const char *logger_name = "exchange_core";
    auto new_logger = spdlog::stdout_color_mt(logger_name);

    if (!new_logger) {
      throw std::runtime_error(
          "Failed to create logger: spdlog::stdout_color_mt returned nullptr");
    }

    // Format matches Java logback pattern
    new_logger->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%t] [%^%l%$] [%n] %v");
    new_logger->set_level(get_cached_log_level());
    new_logger->flush_on(spdlog::level::warn);

    return new_logger;
  } catch (const std::exception &e) {
    std::cerr << "ERROR: Failed to create logger: " << e.what() << std::endl;
    throw;
  }
}

// Get global logger instance (thread-safe singleton)
// Configure via LOG_LEVEL env var: trace, debug, info, warn, error, off
inline std::shared_ptr<spdlog::logger> get_logger() {
  static std::once_flag init_flag;
  static std::shared_ptr<spdlog::logger> logger;

  std::call_once(init_flag, []() { logger = create_configured_logger(); });

  return logger;
}

// Get logger reference (thread-safe, prevents SIOF)
inline spdlog::logger &get_logger_ref() {
  static std::shared_ptr<spdlog::logger> logger_instance = get_logger();
  return *logger_instance;
}

// Logging macros with thread-local caching (zero overhead after first access)
#define LOG_TRACE(...)                                                         \
  do {                                                                         \
    thread_local spdlog::logger *cached_logger =                               \
        &exchange::core::utils::get_logger_ref();                              \
    if (cached_logger->should_log(spdlog::level::trace)) {                     \
      cached_logger->trace(__VA_ARGS__);                                       \
    }                                                                          \
  } while (0)

#define LOG_DEBUG(...)                                                         \
  do {                                                                         \
    thread_local spdlog::logger *cached_logger =                               \
        &exchange::core::utils::get_logger_ref();                              \
    if (cached_logger->should_log(spdlog::level::debug)) {                     \
      cached_logger->debug(__VA_ARGS__);                                       \
    }                                                                          \
  } while (0)

#define LOG_INFO(...)                                                          \
  do {                                                                         \
    thread_local spdlog::logger *cached_logger =                               \
        &exchange::core::utils::get_logger_ref();                              \
    if (cached_logger->should_log(spdlog::level::info)) {                      \
      cached_logger->info(__VA_ARGS__);                                        \
    }                                                                          \
  } while (0)

#define LOG_WARN(...)                                                          \
  do {                                                                         \
    thread_local spdlog::logger *cached_logger =                               \
        &exchange::core::utils::get_logger_ref();                              \
    if (cached_logger->should_log(spdlog::level::warn)) {                      \
      cached_logger->warn(__VA_ARGS__);                                        \
    }                                                                          \
  } while (0)

#define LOG_ERROR(...)                                                         \
  do {                                                                         \
    thread_local spdlog::logger *cached_logger =                               \
        &exchange::core::utils::get_logger_ref();                              \
    if (cached_logger->should_log(spdlog::level::err)) {                       \
      cached_logger->error(__VA_ARGS__);                                       \
    }                                                                          \
  } while (0)

} // namespace utils
} // namespace core
} // namespace exchange
