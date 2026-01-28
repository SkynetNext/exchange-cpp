# Benchmarks will be added here as development progresses

if(BUILD_BENCHMARKS AND benchmark_FOUND)
    # ART Tree Performance Benchmark (Google Benchmark)
    add_executable(perf_long_adaptive_radix_tree_map
        PerfLongAdaptiveRadixTreeMap.cpp
    )
    target_link_libraries(perf_long_adaptive_radix_tree_map
        PRIVATE
            exchange-cpp
            benchmark::benchmark
            benchmark::benchmark_main
    )

    # ART Tree Java-aligned benchmark: same per-iteration flow as
    # reference/collections PerfLongAdaptiveRadixTreeMap. No Google Benchmark.
    add_executable(perf_long_adaptive_radix_tree_map_java_aligned
        PerfLongAdaptiveRadixTreeMapJavaAligned.cpp
    )
    target_link_libraries(perf_long_adaptive_radix_tree_map_java_aligned
        PRIVATE
            exchange-cpp
    )
    
    # Enable LTO for benchmarks (enables cross-module devirtualization/inlining)
    if(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE)
        set_target_properties(perf_long_adaptive_radix_tree_map PROPERTIES
            INTERPROCEDURAL_OPTIMIZATION_RELEASE TRUE
        )
        set_target_properties(perf_long_adaptive_radix_tree_map_java_aligned PROPERTIES
            INTERPROCEDURAL_OPTIMIZATION_RELEASE TRUE
        )
    endif()
    
    # Optimization flags for benchmarks
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(perf_long_adaptive_radix_tree_map PRIVATE
            $<$<CONFIG:Release>:-O3 -march=native -mtune=native>
        )
        target_compile_options(perf_long_adaptive_radix_tree_map_java_aligned PRIVATE
            $<$<CONFIG:Release>:-O3 -march=native -mtune=native>
        )
    elseif(MSVC)
        target_compile_options(perf_long_adaptive_radix_tree_map PRIVATE
            $<$<CONFIG:Release>:/O2>
        )
        target_compile_options(perf_long_adaptive_radix_tree_map_java_aligned PRIVATE
            $<$<CONFIG:Release>:/O2>
        )
    endif()
endif()

