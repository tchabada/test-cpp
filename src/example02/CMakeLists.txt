find_package(spdlog CONFIG REQUIRED)
find_package(asio CONFIG REQUIRED)

set(SRC_FILES
    watchdog.cpp
    executor.cpp
)

add_library(executor_lib STATIC ${SRC_FILES})
target_link_libraries(executor_lib PRIVATE project_warnings project_options PUBLIC asio::asio spdlog::spdlog)

add_executable(example02 main.cpp)
target_link_libraries(example02 PRIVATE project_warnings project_options executor_lib)