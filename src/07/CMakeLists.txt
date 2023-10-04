find_package(unifex CONFIG REQUIRED)

set(CMAKE_CXX_STANDARD 26)

set_source_files_properties(main.cpp PROPERTIES COMPILE_OPTIONS "-freflection")

add_executable(07 main.cpp)
target_link_libraries(07 PRIVATE project_warnings project_options)
