find_package(GTest CONFIG REQUIRED)

set(SRC_FILES
    test01.cpp
    test02.cpp
    test03.cpp
)

add_executable(tests ${SRC_FILES})
target_link_libraries(tests PRIVATE project_warnings project_options GTest::gtest GTest::gtest_main GTest::gmock GTest::gmock_main executor_lib)

add_test(tests tests)
