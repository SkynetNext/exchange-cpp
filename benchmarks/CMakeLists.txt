# Benchmarks will be added here as development progresses

if(BUILD_BENCHMARKS AND benchmark_FOUND)
    # ART Tree Performance Benchmark
    add_executable(perf_long_adaptive_radix_tree_map
        PerfLongAdaptiveRadixTreeMap.cpp
    )
    target_link_libraries(perf_long_adaptive_radix_tree_map
        PRIVATE
            exchange-cpp
            benchmark::benchmark
            benchmark::benchmark_main
    )
    
    # Enable LTO for benchmarks (enables cross-module devirtualization/inlining)
    if(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE)
        set_target_properties(perf_long_adaptive_radix_tree_map PROPERTIES
            INTERPROCEDURAL_OPTIMIZATION_RELEASE TRUE
        )
    endif()
    
    # Optimization flags for benchmarks
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(perf_long_adaptive_radix_tree_map PRIVATE
            $<$<CONFIG:Release>:-O3 -march=native -mtune=native>
        )
    elseif(MSVC)
        target_compile_options(perf_long_adaptive_radix_tree_map PRIVATE
            $<$<CONFIG:Release>:/O2>
        )
    endif()
endif()

