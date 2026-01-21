# Tests for exchange-cpp

if(BUILD_TESTING AND GTest_FOUND)
    # List of all test executables (will be collected for all_tests target)
    set(ALL_TEST_TARGETS "")

    # Function to apply sanitizer flags to test targets
    # Tests inherit sanitizer from exchange-cpp, but we ensure they also have it directly
    function(apply_sanitizer_to_target target_name)
        if(DEFINED ENV{ENABLE_SANITIZER_TSAN} AND "$ENV{ENABLE_SANITIZER_TSAN}")
            target_compile_options(${target_name} PRIVATE
                -fsanitize=thread
                -fno-omit-frame-pointer
                -fno-sanitize-recover=all
            )
            target_link_options(${target_name} PRIVATE -fsanitize=thread)
        elseif(DEFINED ENV{ENABLE_SANITIZER_ASAN} AND "$ENV{ENABLE_SANITIZER_ASAN}")
            target_compile_options(${target_name} PRIVATE
                -fsanitize=address,undefined
                -fno-omit-frame-pointer
                -fno-sanitize-recover=all
            )
            target_link_options(${target_name} PRIVATE -fsanitize=address,undefined)
        elseif(DEFINED ENV{ENABLE_SANITIZER_MSAN} AND "$ENV{ENABLE_SANITIZER_MSAN}")
            target_compile_options(${target_name} PRIVATE
                -fsanitize=memory,undefined
                -fno-omit-frame-pointer
                -fno-sanitize-recover=all
                -fsanitize-memory-track-origins
            )
            target_link_options(${target_name} PRIVATE -fsanitize=memory,undefined)
        endif()
    endfunction()

    # ============================================================================
    # Test utility library - shared utilities for tests
    # ============================================================================
    add_library(test_utils STATIC
        util/ExchangeTestContainer.cpp
        util/ExecutionTime.cpp
        util/JournalingTestsModule.cpp
        util/LatencyTestsModule.cpp
        util/LatencyTools.cpp
        util/L2MarketDataHelper.cpp
        util/PersistenceTestsModule.cpp
        util/TestConstants.cpp
        util/TestDataParameters.cpp
        util/TestOrdersGenerator.cpp
        util/TestOrdersGeneratorSession.cpp
        util/TestOrdersGeneratorConfig.cpp
        util/ThroughputTestsModule.cpp
        util/UserCurrencyAccountsGenerator.cpp
        steps/OrderStepdefs.cpp
    )
    
    target_link_libraries(test_utils
        PRIVATE
            exchange-cpp
            GTest::gtest
    )
    
    target_include_directories(test_utils
        PUBLIC
            ${CMAKE_CURRENT_SOURCE_DIR}
    )
    
    # Apply sanitizer to test_utils (all tests link to it or exchange-cpp)
    apply_sanitizer_to_target(test_utils)

    # ============================================================================
    # Unit Tests
    # ============================================================================

    # ART Tree tests
    add_executable(test_long_adaptive_radix_tree_map
        collections/art/LongAdaptiveRadixTreeMapTest.cpp
    )
    
    target_link_libraries(test_long_adaptive_radix_tree_map
        PRIVATE
            exchange-cpp
            GTest::gtest
            GTest::gtest_main
    )
    
    add_test(NAME LongAdaptiveRadixTreeMapTest COMMAND test_long_adaptive_radix_tree_map)
    list(APPEND ALL_TEST_TARGETS test_long_adaptive_radix_tree_map)

    # SimpleEventsProcessor tests (requires Google Mock)
    # gmock is built as part of googletest submodule (BUILD_GMOCK is set to ON in main CMakeLists.txt)
    add_executable(test_simple_events_processor
        core/SimpleEventsProcessorTest.cpp
    )
    
    # Use the same GTest targets as the other test for consistency
    target_link_libraries(test_simple_events_processor
        PRIVATE
            exchange-cpp
            GTest::gtest
            GTest::gtest_main
            gmock
    )
    
    add_test(NAME SimpleEventsProcessorTest COMMAND test_simple_events_processor)
    list(APPEND ALL_TEST_TARGETS test_simple_events_processor)

    # OrderBook tests
    add_executable(test_orderbook_naive_impl_exchange
        orderbook/OrderBookBaseTest.cpp
        orderbook/OrderBookNaiveImplExchangeTest.cpp
    )
    
    target_link_libraries(test_orderbook_naive_impl_exchange
        PRIVATE
            exchange-cpp
            test_utils
            GTest::gtest
            GTest::gtest_main
    )
    
    add_test(NAME OrderBookNaiveImplExchangeTest COMMAND test_orderbook_naive_impl_exchange)
    list(APPEND ALL_TEST_TARGETS test_orderbook_naive_impl_exchange)

    add_executable(test_orderbook_naive_impl_margin
        orderbook/OrderBookBaseTest.cpp
        orderbook/OrderBookNaiveImplMarginTest.cpp
    )
    
    target_link_libraries(test_orderbook_naive_impl_margin
        PRIVATE
            exchange-cpp
            test_utils
            GTest::gtest
            GTest::gtest_main
    )
    
    add_test(NAME OrderBookNaiveImplMarginTest COMMAND test_orderbook_naive_impl_margin)
    list(APPEND ALL_TEST_TARGETS test_orderbook_naive_impl_margin)

    # OrderBookDirectImpl tests
    add_executable(test_orderbook_direct_impl_exchange
        orderbook/OrderBookBaseTest.cpp
        orderbook/OrderBookDirectImplTest.cpp
        orderbook/OrderBookDirectImplExchangeTest.cpp
    )
    
    target_link_libraries(test_orderbook_direct_impl_exchange
        PRIVATE
            exchange-cpp
            test_utils
            GTest::gtest
            GTest::gtest_main
    )
    
    add_test(NAME OrderBookDirectImplExchangeTest COMMAND test_orderbook_direct_impl_exchange)
    list(APPEND ALL_TEST_TARGETS test_orderbook_direct_impl_exchange)

    add_executable(test_orderbook_direct_impl_margin
        orderbook/OrderBookBaseTest.cpp
        orderbook/OrderBookDirectImplTest.cpp
        orderbook/OrderBookDirectImplMarginTest.cpp
    )
    
    target_link_libraries(test_orderbook_direct_impl_margin
        PRIVATE
            exchange-cpp
            test_utils
            GTest::gtest
            GTest::gtest_main
    )
    
    add_test(NAME OrderBookDirectImplMarginTest COMMAND test_orderbook_direct_impl_margin)
    list(APPEND ALL_TEST_TARGETS test_orderbook_direct_impl_margin)

    # OrdersBucket tests
    add_executable(test_orders_bucket
        orderbook/OrdersBucketTest.cpp
    )
    
    target_link_libraries(test_orders_bucket
        PRIVATE
            exchange-cpp
            GTest::gtest
            GTest::gtest_main
    )
    
    add_test(NAME OrdersBucketTest COMMAND test_orders_bucket)
    list(APPEND ALL_TEST_TARGETS test_orders_bucket)

    # ============================================================================
    # Example Tests
    # ============================================================================
    
    add_executable(test_examples_core
        examples/ITCoreExample.cpp
    )
    
    target_link_libraries(test_examples_core
        PRIVATE
            exchange-cpp
            GTest::gtest
            GTest::gtest_main
    )
    
    add_test(NAME ITCoreExample COMMAND test_examples_core)
    list(APPEND ALL_TEST_TARGETS test_examples_core)

    # ============================================================================
    # Integration Tests
    # ============================================================================
    
    # ITExchangeCoreIntegrationBasic
    add_executable(test_integration_basic
        integration/ITExchangeCoreIntegration.cpp
        integration/ITExchangeCoreIntegrationBasic.cpp
    )
    target_link_libraries(test_integration_basic
        PRIVATE
            exchange-cpp
            test_utils
            GTest::gtest
            GTest::gtest_main
    )
    add_test(NAME ITExchangeCoreIntegrationBasic COMMAND test_integration_basic)
    list(APPEND ALL_TEST_TARGETS test_integration_basic)

    # ITExchangeCoreIntegrationLatency
    add_executable(test_integration_latency
        integration/ITExchangeCoreIntegration.cpp
        integration/ITExchangeCoreIntegrationLatency.cpp
    )
    target_link_libraries(test_integration_latency
        PRIVATE
            exchange-cpp
            test_utils
            GTest::gtest
            GTest::gtest_main
    )
    add_test(NAME ITExchangeCoreIntegrationLatency COMMAND test_integration_latency)
    list(APPEND ALL_TEST_TARGETS test_integration_latency)

    # ITFeesExchangeBasic
    add_executable(test_integration_fees_exchange_basic
        integration/ITFeesExchange.cpp
        integration/ITFeesExchangeBasic.cpp
    )
    target_link_libraries(test_integration_fees_exchange_basic
        PRIVATE
            exchange-cpp
            test_utils
            GTest::gtest
            GTest::gtest_main
    )
    add_test(NAME ITFeesExchangeBasic COMMAND test_integration_fees_exchange_basic)
    list(APPEND ALL_TEST_TARGETS test_integration_fees_exchange_basic)

    # ITFeesExchangeLatency
    add_executable(test_integration_fees_exchange_latency
        integration/ITFeesExchange.cpp
        integration/ITFeesExchangeLatency.cpp
        integration/ITFeesExchangeBasic.cpp
    )
    target_link_libraries(test_integration_fees_exchange_latency
        PRIVATE
            exchange-cpp
            test_utils
            GTest::gtest
            GTest::gtest_main
    )
    add_test(NAME ITFeesExchangeLatency COMMAND test_integration_fees_exchange_latency)
    list(APPEND ALL_TEST_TARGETS test_integration_fees_exchange_latency)

    # ITFeesMarginBasic
    add_executable(test_integration_fees_margin_basic
        integration/ITFeesMargin.cpp
        integration/ITFeesMarginBasic.cpp
    )
    target_link_libraries(test_integration_fees_margin_basic
        PRIVATE
            exchange-cpp
            test_utils
            GTest::gtest
            GTest::gtest_main
    )
    add_test(NAME ITFeesMarginBasic COMMAND test_integration_fees_margin_basic)
    list(APPEND ALL_TEST_TARGETS test_integration_fees_margin_basic)

    # ITFeesMarginLatency
    add_executable(test_integration_fees_margin_latency
        integration/ITFeesMargin.cpp
        integration/ITFeesMarginLatency.cpp
        integration/ITFeesMarginBasic.cpp
    )
    target_link_libraries(test_integration_fees_margin_latency
        PRIVATE
            exchange-cpp
            test_utils
            GTest::gtest
            GTest::gtest_main
    )
    add_test(NAME ITFeesMarginLatency COMMAND test_integration_fees_margin_latency)
    list(APPEND ALL_TEST_TARGETS test_integration_fees_margin_latency)

    # ITExchangeCoreIntegrationRejectionBasic
    add_executable(test_integration_rejection_basic
        integration/ITExchangeCoreIntegrationRejection.cpp
        integration/ITExchangeCoreIntegrationRejectionBasic.cpp
    )
    target_link_libraries(test_integration_rejection_basic
        PRIVATE
            exchange-cpp
            test_utils
            GTest::gtest
            GTest::gtest_main
            GTest::gmock
    )
    add_test(NAME ITExchangeCoreIntegrationRejectionBasic COMMAND test_integration_rejection_basic)
    list(APPEND ALL_TEST_TARGETS test_integration_rejection_basic)

    # ITExchangeCoreIntegrationRejectionLatency
    add_executable(test_integration_rejection_latency
        integration/ITExchangeCoreIntegrationRejection.cpp
        integration/ITExchangeCoreIntegrationRejectionLatency.cpp
        integration/ITExchangeCoreIntegrationRejectionBasic.cpp
    )
    target_link_libraries(test_integration_rejection_latency
        PRIVATE
            exchange-cpp
            test_utils
            GTest::gtest
            GTest::gtest_main
            GTest::gmock
    )
    add_test(NAME ITExchangeCoreIntegrationRejectionLatency COMMAND test_integration_rejection_latency)
    list(APPEND ALL_TEST_TARGETS test_integration_rejection_latency)

    # ITExchangeCoreIntegrationStressBasic
    add_executable(test_integration_stress_basic
        integration/ITExchangeCoreIntegrationStress.cpp
        integration/ITExchangeCoreIntegrationStressBasic.cpp
    )
    target_link_libraries(test_integration_stress_basic
        PRIVATE
            exchange-cpp
            test_utils
            GTest::gtest
            GTest::gtest_main
    )
    add_test(NAME ITExchangeCoreIntegrationStressBasic COMMAND test_integration_stress_basic)
    list(APPEND ALL_TEST_TARGETS test_integration_stress_basic)

    # ITExchangeCoreIntegrationStressLatency
    add_executable(test_integration_stress_latency
        integration/ITExchangeCoreIntegrationStress.cpp
        integration/ITExchangeCoreIntegrationStressLatency.cpp
        integration/ITExchangeCoreIntegrationStressBasic.cpp
    )
    target_link_libraries(test_integration_stress_latency
        PRIVATE
            exchange-cpp
            test_utils
            GTest::gtest
            GTest::gtest_main
    )
    add_test(NAME ITExchangeCoreIntegrationStressLatency COMMAND test_integration_stress_latency)
    list(APPEND ALL_TEST_TARGETS test_integration_stress_latency)

    # ITMultiOperation
    add_executable(test_integration_multi_operation
        integration/ITMultiOperation.cpp
    )
    target_link_libraries(test_integration_multi_operation
        PRIVATE
            exchange-cpp
            test_utils
            GTest::gtest
            GTest::gtest_main
    )
    add_test(NAME ITMultiOperation COMMAND test_integration_multi_operation)
    list(APPEND ALL_TEST_TARGETS test_integration_multi_operation)

    # ============================================================================
    # Cucumber-style BDD Tests (using GTest, matching Java Cucumber tests)
    # ============================================================================

    # RunCukesDirectThroughputTests
    add_executable(test_run_cukes_direct_throughput
        steps/RunCukesDirectThroughputTests.cpp
    )
    target_link_libraries(test_run_cukes_direct_throughput
        PRIVATE
            exchange-cpp
            test_utils
            GTest::gtest
            GTest::gtest_main
    )
    add_test(NAME RunCukesDirectThroughputTests COMMAND test_run_cukes_direct_throughput)
    list(APPEND ALL_TEST_TARGETS test_run_cukes_direct_throughput)

    # RunCukesDirectLatencyTests
    add_executable(test_run_cukes_direct_latency
        steps/RunCukesDirectLatencyTests.cpp
    )
    target_link_libraries(test_run_cukes_direct_latency
        PRIVATE
            exchange-cpp
            test_utils
            GTest::gtest
            GTest::gtest_main
    )
    add_test(NAME RunCukesDirectLatencyTests COMMAND test_run_cukes_direct_latency)
    list(APPEND ALL_TEST_TARGETS test_run_cukes_direct_latency)

    # RunCukeNaiveTests
    add_executable(test_run_cuke_naive
        steps/RunCukeNaiveTests.cpp
    )
    target_link_libraries(test_run_cuke_naive
        PRIVATE
            exchange-cpp
            test_utils
            GTest::gtest
            GTest::gtest_main
    )
    add_test(NAME RunCukeNaiveTests COMMAND test_run_cuke_naive)
    list(APPEND ALL_TEST_TARGETS test_run_cuke_naive)
    
    # ITMultiOperation
    # add_executable(test_integration_multi_operation
    #     integration/ITMultiOperation.cpp
    # )
    # target_link_libraries(test_integration_multi_operation
    #     PRIVATE
    #         exchange-cpp
    #         test_utils
    #         GTest::gtest
    #         GTest::gtest_main
    # )
    # add_test(NAME ITMultiOperation COMMAND test_integration_multi_operation)
    # list(APPEND ALL_TEST_TARGETS test_integration_multi_operation)

    # ============================================================================
    # Performance Tests
    # ============================================================================
    
    # PerfThroughput
    add_executable(test_perf_throughput
        perf/PerfThroughput.cpp
    )
    target_link_libraries(test_perf_throughput
        PRIVATE
            exchange-cpp
            test_utils
            GTest::gtest
            GTest::gtest_main
    )
    add_test(NAME PerfThroughput COMMAND test_perf_throughput)
    list(APPEND ALL_TEST_TARGETS test_perf_throughput)
    
    # PerfLatency
    add_executable(test_perf_latency
        perf/PerfLatency.cpp
    )
    target_link_libraries(test_perf_latency
        PRIVATE
            exchange-cpp
            test_utils
            GTest::gtest
            GTest::gtest_main
    )
    add_test(NAME PerfLatency COMMAND test_perf_latency)
    list(APPEND ALL_TEST_TARGETS test_perf_latency)
    
    # PerfJournaling
    add_executable(test_perf_journaling
        perf/PerfJournaling.cpp
    )
    target_link_libraries(test_perf_journaling
        PRIVATE
            exchange-cpp
            test_utils
            GTest::gtest
            GTest::gtest_main
    )
    add_test(NAME PerfJournaling COMMAND test_perf_journaling)
    list(APPEND ALL_TEST_TARGETS test_perf_journaling)
    
    # PerfPersistence
    add_executable(test_perf_persistence
        perf/PerfPersistence.cpp
    )
    target_link_libraries(test_perf_persistence
        PRIVATE
            exchange-cpp
            test_utils
            GTest::gtest
            GTest::gtest_main
    )
    add_test(NAME PerfPersistence COMMAND test_perf_persistence)
    list(APPEND ALL_TEST_TARGETS test_perf_persistence)
    
    # PerfLatencyJournaling
    add_executable(test_perf_latency_journaling
        perf/PerfLatencyJournaling.cpp
    )
    target_link_libraries(test_perf_latency_journaling
        PRIVATE
            exchange-cpp
            test_utils
            GTest::gtest
            GTest::gtest_main
    )
    add_test(NAME PerfLatencyJournaling COMMAND test_perf_latency_journaling)
    list(APPEND ALL_TEST_TARGETS test_perf_latency_journaling)
    
    # PerfThroughputJournaling
    add_executable(test_perf_throughput_journaling
        perf/PerfThroughputJournaling.cpp
    )
    target_link_libraries(test_perf_throughput_journaling
        PRIVATE
            exchange-cpp
            test_utils
            GTest::gtest
            GTest::gtest_main
    )
    add_test(NAME PerfThroughputJournaling COMMAND test_perf_throughput_journaling)
    list(APPEND ALL_TEST_TARGETS test_perf_throughput_journaling)

    # Apply sanitizer to all test targets
    # This ensures all test executables are linked with sanitizer libraries
    foreach(test_target ${ALL_TEST_TARGETS})
        apply_sanitizer_to_target(${test_target})
    endforeach()

    # Create a custom target that builds all tests
    # This allows CMake Tools to build all tests at once
    add_custom_target(all_tests
        DEPENDS ${ALL_TEST_TARGETS}
        COMMENT "Building all test executables"
    )

    # Make all_tests depend on the main library to ensure it's built first
    add_dependencies(all_tests exchange-cpp)
endif()

