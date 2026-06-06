cmake_minimum_required(VERSION 3.20)

project(mini_coro LANGUAGES CXX)

add_library(mini_coro_headers INTERFACE)
target_compile_features(mini_coro_headers INTERFACE cxx_std_20)
target_include_directories(mini_coro_headers INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/src)

add_executable(mini_coro
    src/main.cpp
)
target_link_libraries(mini_coro PRIVATE mini_coro_headers)

include(CTest)

if(BUILD_TESTING)
    find_package(GTest REQUIRED)

    add_executable(mini_coro_tests
        test/SchedulerTest.cpp
        test/TaskTest.cpp
    )
    target_link_libraries(mini_coro_tests PRIVATE
        mini_coro_headers
        GTest::gtest_main
    )

    include(GoogleTest)
    gtest_discover_tests(mini_coro_tests)
endif()
