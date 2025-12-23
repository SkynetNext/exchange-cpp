# Tests for exchange-cpp

if(BUILD_TESTING AND GTest_FOUND)
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
endif()

