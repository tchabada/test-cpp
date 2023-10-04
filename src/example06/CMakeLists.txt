find_package(benchmark CONFIG REQUIRED)
find_package(Boost REQUIRED COMPONENTS lockfree)

add_executable(example06 main.cpp)
target_link_libraries(example06 PRIVATE project_warnings project_options benchmark::benchmark benchmark::benchmark_main Boost::lockfree)
