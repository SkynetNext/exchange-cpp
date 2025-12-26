/*
 * FutureDebugHelper - Helper to debug std::future_error exceptions
 * 
 * This header provides macros and utilities to wrap future.get() calls
 * and provide detailed error information when std::future_error is thrown.
 */

#pragma once

#include <future>
#include <stdexcept>
#include <string>
#include <sstream>

namespace exchange {
namespace core {
namespace tests {
namespace util {

/**
 * Helper function to get future value with detailed error reporting (move version)
 */
template<typename T>
T GetFutureWithDebug(std::future<T>&& future, const char* file, int line, const char* function, const char* context = "") {
  try {
    return future.get();
  } catch (const std::future_error& e) {
    std::stringstream ss;
    ss << "std::future_error at " << file << ":" << line << " in " << function;
    if (context && *context) {
      ss << " [" << context << "]";
    }
    ss << "\n  Error code: " << static_cast<int>(e.code().value());
    ss << "\n  Error message: " << e.what();
    
    if (e.code() == std::future_errc::no_state) {
      ss << "\n  This means the future/promise has no associated shared state.";
      ss << "\n  Common causes:";
      ss << "\n    1. Promise was destroyed before set_value() was called";
      ss << "\n    2. Future was moved/copied incorrectly";
      ss << "\n    3. ExchangeCore was shut down before promise was fulfilled";
    } else if (e.code() == std::future_errc::broken_promise) {
      ss << "\n  This means the promise was broken (destroyed without setting a value).";
    }
    
    throw std::runtime_error(ss.str());
  }
}

/**
 * Helper function to get future value with detailed error reporting (reference version, for reusable futures)
 */
template<typename T>
T GetFutureWithDebugRef(std::future<T>& future, const char* file, int line, const char* function, const char* context = "") {
  try {
    return future.get();
  } catch (const std::future_error& e) {
    std::stringstream ss;
    ss << "std::future_error at " << file << ":" << line << " in " << function;
    if (context && *context) {
      ss << " [" << context << "]";
    }
    ss << "\n  Error code: " << static_cast<int>(e.code().value());
    ss << "\n  Error message: " << e.what();
    
    if (e.code() == std::future_errc::no_state) {
      ss << "\n  This means the future/promise has no associated shared state.";
      ss << "\n  Common causes:";
      ss << "\n    1. Promise was destroyed before set_value() was called";
      ss << "\n    2. Future was moved/copied incorrectly";
      ss << "\n    3. ExchangeCore was shut down before promise was fulfilled";
    } else if (e.code() == std::future_errc::broken_promise) {
      ss << "\n  This means the promise was broken (destroyed without setting a value).";
    }
    
    throw std::runtime_error(ss.str());
  }
}

/**
 * Macro to wrap future.get() with debug information
 */
#define GET_FUTURE_DEBUG(future, context) \
  exchange::core::tests::util::GetFutureWithDebug( \
    std::move(future), __FILE__, __LINE__, __FUNCTION__, context)

/**
 * Macro for simple future.get() with debug
 */
#define GET_FUTURE(future) GET_FUTURE_DEBUG(future, "")

/**
 * Macro to wrap future.get() with debug information (for reusable futures, no move)
 */
#define GET_FUTURE_DEBUG_REF(future, context) \
  exchange::core::tests::util::GetFutureWithDebugRef( \
    future, __FILE__, __LINE__, __FUNCTION__, context)

} // namespace util
} // namespace tests
} // namespace core
} // namespace exchange

