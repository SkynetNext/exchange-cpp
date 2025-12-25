cmake_minimum_required(VERSION 3.20)
project(exchange-cpp VERSION 0.1.0 LANGUAGES CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 20)
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

# Enable testing support (required for CTest)
enable_testing()

# Build options
option(BUILD_TESTING "Build tests" ON)
option(BUILD_BENCHMARKS "Build benchmarks" ON)
option(BUILD_EXAMPLES "Build examples" ON)

# Include third-party dependencies (add disruptor-cpp first as it may provide googletest/benchmark)

# disruptor-cpp: High-performance inter-thread communication
# Note: disruptor-cpp may add googletest and benchmark as submodules if BUILD_BENCHMARKS is ON
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/third_party/disruptor-cpp/CMakeLists.txt)
    # Temporarily disable disruptor-cpp's own tests to avoid build errors
    # We only need the library, not its tests
    set(BUILD_TESTING_OLD ${BUILD_TESTING})
    set(BUILD_TESTING OFF)
    add_subdirectory(third_party/disruptor-cpp)
    set(BUILD_TESTING ${BUILD_TESTING_OLD})
    message(STATUS "Using disruptor-cpp from submodule (tests disabled)")
else()
    message(FATAL_ERROR "disruptor-cpp submodule not found. Please run: git submodule update --init --recursive")
endif()

# Enable testing (check if GTest is already available from disruptor-cpp)
if(BUILD_TESTING)
    # Check if GTest is already available (e.g., from disruptor-cpp)
    if(TARGET gtest AND TARGET gtest_main)
        set(GTest_FOUND TRUE)
        message(STATUS "Using GoogleTest from disruptor-cpp")
    elseif(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/third_party/googletest/CMakeLists.txt)
        # Ensure BUILD_GMOCK is ON so gmock target is available
        set(BUILD_GMOCK ON CACHE BOOL "Builds the googlemock subproject" FORCE)
        add_subdirectory(third_party/googletest)
        set(GTest_FOUND TRUE)
        message(STATUS "Using GoogleTest from submodule")
    else()
        find_package(GTest QUIET)
        if(NOT GTest_FOUND)
            message(STATUS "GTest not found, tests will be skipped")
            set(BUILD_TESTING OFF)
        else()
            message(STATUS "Using GTest from system")
        endif()
    endif()
endif()

# ankerl::unordered_dense: Fastest hash map (header-only)
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/third_party/unordered_dense/include/ankerl/unordered_dense.h)
    message(STATUS "Using ankerl::unordered_dense from submodule")
else()
    message(WARNING "ankerl::unordered_dense not found. Please run: git submodule add https://github.com/martinus/unordered_dense.git third_party/unordered_dense")
endif()

# mimalloc: High-performance memory allocator
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/third_party/mimalloc/CMakeLists.txt)
    # Disable mimalloc tests and examples to avoid ctest conflicts
    set(MI_BUILD_TESTS OFF CACHE BOOL "Build mimalloc tests" FORCE)
    set(MI_BUILD_EXAMPLES OFF CACHE BOOL "Build mimalloc examples" FORCE)
    set(MI_BUILD_SHARED OFF CACHE BOOL "Build shared library" FORCE)
    set(MI_BUILD_STATIC ON CACHE BOOL "Build static library" FORCE)
    add_subdirectory(third_party/mimalloc)
    message(STATUS "Using mimalloc from submodule (tests disabled)")
else()
    message(WARNING "mimalloc submodule not found. Please run: git submodule add https://github.com/microsoft/mimalloc.git third_party/mimalloc")
endif()

# lz4: Fast compression library (required dependency, 1:1 with Java version)
# Java version uses lz4-java as required dependency, so C++ version should too
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/third_party/lz4/lib/lz4.h)
    add_subdirectory(third_party/lz4/build/cmake)
    message(STATUS "Using lz4 from submodule")
else()
    find_package(lz4 REQUIRED)
    if(lz4_FOUND)
        message(STATUS "Using lz4 from system")
    else()
        message(FATAL_ERROR "lz4 not found. LZ4 is a required dependency (1:1 with Java version).")
        message(FATAL_ERROR "Please run: git submodule add https://github.com/lz4/lz4.git third_party/lz4")
        message(FATAL_ERROR "Or install: apt-get install liblz4-dev (Ubuntu) or brew install lz4 (macOS)")
    endif()
endif()

# Intel TBB: High-performance concurrent data structures
# Used for lock-free promise map (better than Java ConcurrentHashMap)
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/third_party/tbb/CMakeLists.txt)
    message(STATUS "Using Intel TBB from submodule")
    # Disable position-independent code on Windows (fPIC not supported on Windows/MSVC)
    if(WIN32)
        set(CMAKE_POSITION_INDEPENDENT_CODE OFF CACHE BOOL "Position independent code" FORCE)
    endif()
    # Disable TBB tests to avoid ctest conflicts
    set(TBB_TEST OFF CACHE BOOL "Enable testing" FORCE)
    # Build TBB as static library to avoid DLL dependency issues at runtime
    # Note: TBB warns about static library, but for HFT systems, static linking
    # is preferred (no DLL dependencies, better performance, easier deployment)
    set(BUILD_SHARED_LIBS_SAVED ${BUILD_SHARED_LIBS})
    set(BUILD_SHARED_LIBS OFF CACHE BOOL "Build shared libraries" FORCE)
    add_subdirectory(third_party/tbb)
    set(BUILD_SHARED_LIBS ${BUILD_SHARED_LIBS_SAVED} CACHE BOOL "Build shared libraries" FORCE)
    set(TBB_TARGET TBB::tbb)
else()
    message(WARNING "Intel TBB submodule not found. Please run:")
    message(WARNING "  git submodule add https://github.com/oneapi-src/oneTBB.git third_party/tbb")
    message(FATAL_ERROR "TBB is required for lock-free promise map")
endif()

# spdlog: High-performance logging library (header-only, matches Java SLF4J+Logback)
# Using FetchContent for easy integration
include(FetchContent)
FetchContent_Declare(
    spdlog
    GIT_REPOSITORY https://github.com/gabime/spdlog.git
    GIT_TAG        v1.12.0
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(spdlog)
message(STATUS "Using spdlog for logging (synchronous mode, stdout output, matches Java version)")

# Collect source files from subdirectories
# Note: GLOB_RECURSE recursively searches subdirectories
file(GLOB_RECURSE EXCHANGE_SOURCES
    # Files directly in core/ directory
    "${CMAKE_CURRENT_SOURCE_DIR}/src/exchange/core/*.cpp"
    # Subdirectories (already covered by GLOB_RECURSE above, but kept for clarity)
    "${CMAKE_CURRENT_SOURCE_DIR}/src/exchange/core/common/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/exchange/core/orderbook/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/exchange/core/processors/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/exchange/core/processors/journaling/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/exchange/core/collections/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/exchange/core/utils/*.cpp"
)

# Explicitly ensure new files are included (in case GLOB_RECURSE cache is stale)
list(APPEND EXCHANGE_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/src/exchange/core/processors/journaling/DummySerializationProcessor.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/exchange/core/utils/AffinityThreadFactory.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/exchange/core/common/api/reports/TotalCurrencyBalanceReportResult.cpp"
)

# Create library (placeholder for now)
add_library(exchange-cpp STATIC
    ${EXCHANGE_SOURCES}
)

# Include directories - use target_include_directories for better IDE support
target_include_directories(exchange-cpp
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        # Third-party header-only libraries
        ${CMAKE_CURRENT_SOURCE_DIR}/third_party/unordered_dense/include
        ${CMAKE_CURRENT_SOURCE_DIR}/third_party/tbb/include
    PRIVATE
        # Private includes if needed
)

target_link_libraries(exchange-cpp
    PUBLIC
        disruptor-cpp
        Threads::Threads
        spdlog::spdlog
        ${TBB_TARGET}
        # ART tree is custom implementation, no external dependency
)

# Link mimalloc if available as submodule
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/third_party/mimalloc/CMakeLists.txt)
    target_link_libraries(exchange-cpp PUBLIC mimalloc-static)
    message(STATUS "Linking mimalloc-static for high-performance memory allocation")
endif()

# Link lz4 if available
if(TARGET lz4)
    target_link_libraries(exchange-cpp PUBLIC lz4)
    message(STATUS "Linking lz4 for compression support")
elseif(lz4_FOUND)
    target_link_libraries(exchange-cpp PUBLIC ${LZ4_LIBRARIES})
    target_include_directories(exchange-cpp PRIVATE ${LZ4_INCLUDE_DIRS})
    message(STATUS "Linking system lz4 for compression support")
endif()

target_compile_features(exchange-cpp PUBLIC cxx_std_20)

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
    # Check if benchmark is already available (e.g., from disruptor-cpp)
    if(TARGET benchmark AND TARGET benchmark_main)
        set(benchmark_FOUND TRUE)
        message(STATUS "Using Google Benchmark from disruptor-cpp")
    elseif(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/third_party/benchmark/CMakeLists.txt)
        # Disable benchmark's own tests and examples
        set(BENCHMARK_ENABLE_TESTING OFF CACHE BOOL "" FORCE)
        set(BENCHMARK_ENABLE_GTEST_TESTS OFF CACHE BOOL "" FORCE)
        set(BENCHMARK_ENABLE_INSTALL OFF CACHE BOOL "" FORCE)
        add_subdirectory(third_party/benchmark)
        set(benchmark_FOUND TRUE)
        message(STATUS "Using Google Benchmark from submodule")
    else()
        find_package(benchmark QUIET)
        if(benchmark_FOUND)
            message(STATUS "Using Google Benchmark from system")
        else()
            message(STATUS "Google Benchmark not found, benchmarks will be skipped")
        endif()
    endif()
    
    if(benchmark_FOUND)
        add_subdirectory(benchmarks)
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

