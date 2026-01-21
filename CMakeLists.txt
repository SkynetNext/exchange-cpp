cmake_minimum_required(VERSION 3.30)
project(exchange-cpp VERSION 0.1.0 LANGUAGES CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Compiler flags for performance
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    # Enable LTO with parallel processing for faster link time
    # -flto=auto: Use all available CPU cores for LTO optimization
    # This reduces the "serial compilation" warnings and speeds up linking
    set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG -march=native -mtune=native -flto=auto")
    set(CMAKE_CXX_FLAGS_DEBUG "-O0 -g3 -DDEBUG")
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    set(CMAKE_CXX_FLAGS_RELEASE "/O2 /DNDEBUG")
    set(CMAKE_CXX_FLAGS_DEBUG "/Od /Zi /DDEBUG")
endif()

# Export compile commands for clang-tidy and other tools
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Set default build type to Release
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Choose the type of build." FORCE)
endif()

# Enable Link Time Optimization (LTO) for Release builds
# LTO can provide 10-30% performance improvement by enabling cross-module inlining
# and better optimization across translation units
include(CheckIPOSupported)
check_ipo_supported(RESULT IPO_SUPPORTED OUTPUT error LANGUAGES CXX)

if(IPO_SUPPORTED)
    # Enable LTO for Release builds
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE ON)
    message(STATUS "Link Time Optimization (LTO) is ENABLED for Release builds")
    message(STATUS "  Note: LTO will significantly increase build time and memory usage")
    message(STATUS "  Expected performance improvement: 10-30% (especially for P90 latency)")
else()
    message(WARNING "Link Time Optimization (LTO) is NOT supported: ${error}")
    message(WARNING "  LTO requires CMake 3.9+ and a compatible compiler (GCC 4.9+, Clang 3.9+)")
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
    # Temporarily disable disruptor-cpp's own tests and benchmarks to avoid build errors
    # We only need the library, not its tests or benchmarks
    set(BUILD_TESTING_OLD ${BUILD_TESTING})
    set(BUILD_BENCHMARKS_OLD ${BUILD_BENCHMARKS})
    set(BUILD_TESTING OFF)
    set(BUILD_BENCHMARKS OFF)
    add_subdirectory(third_party/disruptor-cpp)
    set(BUILD_TESTING ${BUILD_TESTING_OLD})
    set(BUILD_BENCHMARKS ${BUILD_BENCHMARKS_OLD})
    message(STATUS "Using disruptor-cpp from submodule (tests and benchmarks disabled)")
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
    # Temporarily disable LTO for mimalloc to avoid "plugin needed to handle lto object" errors
    # Clang's LTO requires all objects to be LTO or none, mixing causes linker plugin issues
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_SAVED ${CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE})
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE OFF)
    add_subdirectory(third_party/mimalloc)
    # Restore LTO setting for main project
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE ${CMAKE_INTERPROCEDURAL_OPTIMIZATION_SAVED})
    message(STATUS "Using mimalloc from submodule (tests disabled, LTO disabled for compatibility)")
else()
    message(WARNING "mimalloc submodule not found. Please run: git submodule add https://github.com/microsoft/mimalloc.git third_party/mimalloc")
endif()

# lz4: Fast compression library (required dependency, 1:1 with Java version)
# Java version uses lz4-java as required dependency, so C++ version should too
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/third_party/lz4/lib/lz4.h)
    # Temporarily disable LTO for lz4 to avoid "plugin needed to handle lto object" errors
    # Clang's LTO requires all objects to be LTO or none, mixing causes linker plugin issues
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_SAVED ${CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE})
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE OFF)
    add_subdirectory(third_party/lz4/build/cmake)
    # Restore LTO setting for main project
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE ${CMAKE_INTERPROCEDURAL_OPTIMIZATION_SAVED})
    message(STATUS "Using lz4 from submodule (LTO disabled for compatibility)")
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
    # Disable TBB warnings as errors (TSAN compatibility issue with atomic_thread_fence)
    set(TBB_STRICT OFF CACHE BOOL "Treat compiler warnings as errors" FORCE)
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

# moodycamel::concurrentqueue: High-performance lock-free queue
# Header-only library, faster than TBB concurrent_queue for high-frequency trading
# Replaces std::queue + std::mutex in SharedPool for better performance
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/third_party/concurrentqueue/concurrentqueue.h)
    message(STATUS "Using moodycamel::concurrentqueue from submodule")
    set(CONCURRENTQUEUE_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/third_party/concurrentqueue)
else()
    include(FetchContent)
    FetchContent_Declare(
        concurrentqueue
        GIT_REPOSITORY https://github.com/cameron314/concurrentqueue.git
        GIT_TAG        v1.0.4
        GIT_SHALLOW    TRUE
    )
    FetchContent_GetProperties(concurrentqueue)
    if(NOT concurrentqueue_POPULATED)
        FetchContent_Populate(concurrentqueue)
        set(CONCURRENTQUEUE_INCLUDE_DIR ${concurrentqueue_SOURCE_DIR})
        message(STATUS "Using moodycamel::concurrentqueue from FetchContent")
    else()
        # Already populated, get the source directory
        FetchContent_GetProperties(concurrentqueue SOURCE_DIR concurrentqueue_SOURCE_DIR)
        set(CONCURRENTQUEUE_INCLUDE_DIR ${concurrentqueue_SOURCE_DIR})
    endif()
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
    "${CMAKE_CURRENT_SOURCE_DIR}/src/exchange/core/common/config/SerializationConfiguration.cpp"
)

# Create library (placeholder for now)
add_library(exchange-cpp STATIC
    ${EXCHANGE_SOURCES}
)

# Enable LTO for exchange-cpp target if supported
if(IPO_SUPPORTED)
    set_target_properties(exchange-cpp PROPERTIES
        INTERPROCEDURAL_OPTIMIZATION_RELEASE TRUE
    )
endif()

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

# Add concurrentqueue include directory (header-only library)
if(CONCURRENTQUEUE_INCLUDE_DIR)
    target_include_directories(exchange-cpp PRIVATE ${CONCURRENTQUEUE_INCLUDE_DIR})
    message(STATUS "Added concurrentqueue include directory: ${CONCURRENTQUEUE_INCLUDE_DIR}")
endif()

target_link_libraries(exchange-cpp
    PUBLIC
        disruptor-cpp
        Threads::Threads
        spdlog::spdlog
        ${TBB_TARGET}
        # ART tree is custom implementation, no external dependency
)

# Link mimalloc if available as submodule
# Note: LTO is disabled during mimalloc build (see add_subdirectory above)
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/third_party/mimalloc/CMakeLists.txt)
    target_link_libraries(exchange-cpp PUBLIC mimalloc-static)
    message(STATUS "Linking mimalloc-static for high-performance memory allocation")
endif()

# Link lz4 if available
# Note: LTO is disabled during lz4 build (see add_subdirectory above)
if(TARGET lz4)
    target_link_libraries(exchange-cpp PUBLIC lz4)
    message(STATUS "Linking lz4 for compression support")
elseif(lz4_FOUND)
    target_link_libraries(exchange-cpp PUBLIC ${LZ4_LIBRARIES})
    target_include_directories(exchange-cpp PRIVATE ${LZ4_INCLUDE_DIRS})
    message(STATUS "Linking system lz4 for compression support")
endif()

target_compile_features(exchange-cpp PUBLIC cxx_std_26)

# Warning flags: treat warnings as errors, but suppress unused parameter warnings
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(exchange-cpp PRIVATE
        -Werror
        -Wno-unused-parameter
    )
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    target_compile_options(exchange-cpp PRIVATE /WX)
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
        # Workaround for Windows/Clang: pre-detect std::regex support
        # benchmark's cxx_feature_check will use this if already defined
        if(WIN32 AND CMAKE_CXX_COMPILER_ID MATCHES "Clang")
            # On Windows, Clang typically uses MSVC standard library which supports std::regex
            # However, the detection may fail due to linking issues. Try to detect first,
            # but if it fails, assume it's available (common case) and let benchmark handle it
            include(CheckCXXSourceCompiles)
            # Use same test as benchmark's std_regex.cpp
            check_cxx_source_compiles(
                "#include <regex>
                 #include <string>
                 int main() {
                     const std::string str = \"test0159\";
                     std::regex re;
                     re = std::regex(\"^[a-z]+[0-9]+$\",
                          std::regex_constants::extended | std::regex_constants::nosubs);
                     return std::regex_search(str, re) ? 0 : -1;
                 }"
                HAVE_STD_REGEX_DETECTED
            )
            if(HAVE_STD_REGEX_DETECTED)
                set(HAVE_STD_REGEX ON CACHE BOOL "std::regex support" FORCE)
                message(STATUS "Pre-detected std::regex for benchmark (Windows/Clang workaround)")
            else()
                # Detection failed, but on Windows/Clang with MSVC stdlib, std::regex should work
                # Set it anyway and let benchmark's own detection handle edge cases
                # This is a workaround for cases where check_cxx_source_compiles fails
                # due to linking issues but std::regex is actually available at runtime
                set(HAVE_STD_REGEX ON CACHE BOOL "std::regex support (assumed for Windows/Clang)" FORCE)
                message(STATUS "Assuming std::regex available for benchmark (Windows/Clang workaround)")
            endif()
        endif()
        add_subdirectory(third_party/benchmark)
        # Suppress format-nonliteral warning for Google Benchmark (known issue in older versions)
        # This warning occurs in third_party/benchmark/src/string_util.cc
        # Google Benchmark's CMakeLists.txt sets -Wformat=2 and -Werror.
        # We need to add -Wno-format-nonliteral AFTER all other options so it takes precedence.
        if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
            if(TARGET benchmark)
                # Get current compile options
                get_target_property(_benchmark_compile_options benchmark COMPILE_OPTIONS)
                # Add -Wno-format-nonliteral at the end to override -Wformat=2
                if(_benchmark_compile_options)
                    list(APPEND _benchmark_compile_options "-Wno-format-nonliteral")
                else()
                    set(_benchmark_compile_options "-Wno-format-nonliteral")
                endif()
                set_target_properties(benchmark PROPERTIES COMPILE_OPTIONS "${_benchmark_compile_options}")
                unset(_benchmark_compile_options)
            endif()
            
            if(TARGET benchmark_main)
                get_target_property(_benchmark_main_compile_options benchmark_main COMPILE_OPTIONS)
                if(_benchmark_main_compile_options)
                    list(APPEND _benchmark_main_compile_options "-Wno-format-nonliteral")
                else()
                    set(_benchmark_main_compile_options "-Wno-format-nonliteral")
                endif()
                set_target_properties(benchmark_main PROPERTIES COMPILE_OPTIONS "${_benchmark_main_compile_options}")
                unset(_benchmark_main_compile_options)
            endif()
        endif()
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
if(IPO_SUPPORTED)
    message(STATUS "Link Time Optimization (LTO): ENABLED")
else()
    message(STATUS "Link Time Optimization (LTO): DISABLED")
endif()
message(STATUS "Build tests: ${BUILD_TESTING}")
message(STATUS "Build benchmarks: ${BUILD_BENCHMARKS}")
message(STATUS "Build examples: ${BUILD_EXAMPLES}")
message(STATUS "=================================================")
message(STATUS "")

