find_package(GTest CONFIG REQUIRED)

add_executable(tests test03.cpp test04.cpp)
target_link_libraries(tests PRIVATE project_warnings project_options GTest::gtest GTest::gtest_main GTest::gmock GTest::gmock_main)
add_test(tests tests)
