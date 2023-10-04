find_package(benchmark CONFIG REQUIRED)
find_package(Boost REQUIRED COMPONENTS lockfree)

add_executable(bench_ringbuffer bench_ringbuffer.cpp)
target_link_libraries(bench_ringbuffer PRIVATE project_warnings project_options benchmark::benchmark benchmark::benchmark_main Boost::lockfree)

find_package(Boost REQUIRED COMPONENTS poly_collection)

add_executable(bench_dispatch bench_dispatch.cpp dispatch_compute.cpp)
target_link_libraries(bench_dispatch PRIVATE project_warnings project_options benchmark::benchmark benchmark::benchmark_main Boost::poly_collection)

add_executable(bench_dispatch_lto bench_dispatch.cpp dispatch_compute.cpp)
target_link_libraries(bench_dispatch_lto PRIVATE project_warnings project_options benchmark::benchmark benchmark::benchmark_main Boost::poly_collection)

include(CheckIPOSupported)

check_ipo_supported(RESULT ipo_supported OUTPUT ipo_error)

if(ipo_supported)
    set_property(TARGET bench_dispatch_lto PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)
else()
    message(WARNING "IPO/LTO not supported: ${ipo_error}")
endif()
