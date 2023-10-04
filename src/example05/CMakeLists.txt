find_package(unifex CONFIG REQUIRED)

add_executable(example05 main.cpp)
target_link_libraries(example05 PRIVATE project_warnings project_options unifex::unifex)
