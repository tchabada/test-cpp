find_package(unifex CONFIG REQUIRED)

set_source_files_properties(main.cpp PROPERTIES COMPILE_OPTIONS "-freflection")

add_executable(example07 main.cpp)
target_link_libraries(example07 PRIVATE project_warnings project_options)
