find_package(GTest CONFIG REQUIRED)

add_executable(test_bufferpool test_bufferpool.cpp)
target_link_libraries(test_bufferpool PRIVATE project_warnings project_options GTest::gtest GTest::gtest_main GTest::gmock GTest::gmock_main)

add_executable(test_executor test_executor.cpp)
target_link_libraries(test_executor PRIVATE project_warnings project_options GTest::gtest GTest::gtest_main GTest::gmock GTest::gmock_main executor_lib)

add_executable(test_fsm test_fsm.cpp)
target_link_libraries(test_fsm PRIVATE project_warnings project_options GTest::gtest GTest::gtest_main GTest::gmock GTest::gmock_main)

add_executable(test_mixin test_mixin.cpp)
target_link_libraries(test_mixin PRIVATE project_warnings project_options GTest::gtest GTest::gtest_main GTest::gmock GTest::gmock_main)

add_test(NAME test_bufferpool COMMAND ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/test_bufferpool)
add_test(NAME test_executor COMMAND ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/test_executor)
add_test(NAME test_fsm COMMAND ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/test_fsm)
add_test(NAME test_mixin COMMAND ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/test_mixin)

find_package(benchmark CONFIG REQUIRED)
find_package(Boost REQUIRED COMPONENTS lockfree)

add_executable(bench_ringbuffer bench_ringbuffer.cpp)
target_link_libraries(bench_ringbuffer PRIVATE project_warnings project_options benchmark::benchmark benchmark::benchmark_main Boost::lockfree)
