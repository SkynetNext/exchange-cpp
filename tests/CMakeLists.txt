# Tests for exchange-cpp

if(BUILD_TESTING AND GTest_FOUND)
    # List of all test executables (will be collected for all_tests target)
    set(ALL_TEST_TARGETS "")

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
        util/L2MarketDataHelper.cpp
        util/TestConstants.cpp
    )
    
    target_link_libraries(test_orderbook_naive_impl_exchange
        PRIVATE
            exchange-cpp
            GTest::gtest
            GTest::gtest_main
    )
    
    add_test(NAME OrderBookNaiveImplExchangeTest COMMAND test_orderbook_naive_impl_exchange)
    list(APPEND ALL_TEST_TARGETS test_orderbook_naive_impl_exchange)

    add_executable(test_orderbook_naive_impl_margin
        orderbook/OrderBookBaseTest.cpp
        orderbook/OrderBookNaiveImplMarginTest.cpp
        util/L2MarketDataHelper.cpp
        util/TestConstants.cpp
    )
    
    target_link_libraries(test_orderbook_naive_impl_margin
        PRIVATE
            exchange-cpp
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
        util/L2MarketDataHelper.cpp
        util/TestConstants.cpp
    )
    
    target_link_libraries(test_orderbook_direct_impl_exchange
        PRIVATE
            exchange-cpp
            GTest::gtest
            GTest::gtest_main
    )
    
    add_test(NAME OrderBookDirectImplExchangeTest COMMAND test_orderbook_direct_impl_exchange)
    list(APPEND ALL_TEST_TARGETS test_orderbook_direct_impl_exchange)

    add_executable(test_orderbook_direct_impl_margin
        orderbook/OrderBookBaseTest.cpp
        orderbook/OrderBookDirectImplTest.cpp
        orderbook/OrderBookDirectImplMarginTest.cpp
        util/L2MarketDataHelper.cpp
        util/TestConstants.cpp
    )
    
    target_link_libraries(test_orderbook_direct_impl_margin
        PRIVATE
            exchange-cpp
            GTest::gtest
            GTest::gtest_main
    )
    
    add_test(NAME OrderBookDirectImplMarginTest COMMAND test_orderbook_direct_impl_margin)
    list(APPEND ALL_TEST_TARGETS test_orderbook_direct_impl_margin)

    # Create a custom target that builds all tests
    # This allows CMake Tools to build all tests at once
    add_custom_target(all_tests
        DEPENDS ${ALL_TEST_TARGETS}
        COMMENT "Building all test executables"
    )

    # Make all_tests depend on the main library to ensure it's built first
    add_dependencies(all_tests exchange-cpp)
endif()

