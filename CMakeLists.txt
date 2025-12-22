cmake_minimum_required(VERSION 3.20)
project(exchange-cpp VERSION 0.1.0 LANGUAGES CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Compiler flags for performance
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG -march=native -mtune=native")
    set(CMAKE_CXX_FLAGS_DEBUG "-O0 -g3 -DDEBUG -fsanitize=address")
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    set(CMAKE_CXX_FLAGS_RELEASE "/O2 /DNDEBUG")
    set(CMAKE_CXX_FLAGS_DEBUG "/Od /Zi /DDEBUG")
endif()

# Set default build type to Release
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Choose the type of build." FORCE)
endif()

# Find dependencies
find_package(Threads REQUIRED)

# Build options
option(BUILD_TESTING "Build tests" ON)
option(BUILD_BENCHMARKS "Build benchmarks" ON)
option(BUILD_EXAMPLES "Build examples" ON)

# Enable testing
if(BUILD_TESTING)
    enable_testing()
    find_package(GTest QUIET)
    if(NOT GTest_FOUND)
        message(STATUS "GTest not found, tests will be skipped")
        set(BUILD_TESTING OFF)
    endif()
endif()

# Include third-party dependencies
# disruptor-cpp: High-performance inter-thread communication
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/third_party/disruptor-cpp/CMakeLists.txt)
    add_subdirectory(third_party/disruptor-cpp)
    message(STATUS "Using disruptor-cpp from submodule")
else()
    message(FATAL_ERROR "disruptor-cpp submodule not found. Please run: git submodule update --init --recursive")
endif()

# Include directories
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    # Simplified structure: exchange.core.* (removed core2 version suffix)
    ${CMAKE_CURRENT_SOURCE_DIR}/include/exchange/core
)

# Source directories (will be populated as development progresses)
set(EXCHANGE_SOURCES
    # src/exchange/core/...
    # src/exchange/risk/...
    # src/exchange/api/...
)

# Create library (placeholder for now)
add_library(exchange-cpp STATIC
    ${EXCHANGE_SOURCES}
)

target_link_libraries(exchange-cpp
    PUBLIC
        disruptor-cpp
        Threads::Threads
)

target_compile_features(exchange-cpp PUBLIC cxx_std_17)

# Suppress unused parameter warnings
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(exchange-cpp PRIVATE -Wno-unused-parameter)
endif()

# Install rules
install(TARGETS exchange-cpp
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION bin
)

install(DIRECTORY include/exchange
    DESTINATION include
    FILES_MATCHING PATTERN "*.h" PATTERN "*.hpp"
)

# Tests
if(BUILD_TESTING AND GTest_FOUND)
    add_subdirectory(tests)
endif()

# Benchmarks
if(BUILD_BENCHMARKS)
    find_package(benchmark QUIET)
    if(benchmark_FOUND)
        add_subdirectory(benchmarks)
    else()
        message(STATUS "Google Benchmark not found, benchmarks will be skipped")
    endif()
endif()

# Examples
if(BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

# Print configuration summary
message(STATUS "")
message(STATUS "========== Exchange-CPP Configuration ==========")
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "C++ standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "Build tests: ${BUILD_TESTING}")
message(STATUS "Build benchmarks: ${BUILD_BENCHMARKS}")
message(STATUS "Build examples: ${BUILD_EXAMPLES}")
message(STATUS "=================================================")
message(STATUS "")

