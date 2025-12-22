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
endif()

