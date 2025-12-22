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
endif()

