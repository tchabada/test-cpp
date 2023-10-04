find_package(benchmark CONFIG REQUIRED)
find_package(Boost REQUIRED COMPONENTS lockfree)

add_executable(bench_ringbuffer bench_ringbuffer.cpp)
target_link_libraries(bench_ringbuffer PRIVATE project_warnings project_options benchmark::benchmark benchmark::benchmark_main Boost::lockfree)

find_package(Boost REQUIRED COMPONENTS poly_collection)

add_executable(bench_dispatch bench_dispatch.cpp dispatch_context.cpp dispatch_compute.cpp)
target_link_libraries(bench_dispatch PRIVATE project_warnings project_options benchmark::benchmark benchmark::benchmark_main Boost::poly_collection)
