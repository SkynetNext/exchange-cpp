# Tests for exchange-cpp

if(BUILD_TESTING AND GTest_FOUND)
    # List of all test executables (will be collected for all_tests target)
    set(ALL_TEST_TARGETS "")

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

        # Create a custom target that builds all tests
    # This allows CMake Tools to build all tests at once
    add_custom_target(all_tests
        DEPENDS ${ALL_TEST_TARGETS}
        COMMENT "Building all test executables"
    )

    # Make all_tests depend on the main library to ensure it's built first
    add_dependencies(all_tests exchange-cpp)
endif()

