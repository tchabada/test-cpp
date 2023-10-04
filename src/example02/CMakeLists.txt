find_package(unifex CONFIG REQUIRED)

add_executable(example02 main.cpp)
target_link_libraries(example02 PRIVATE project_warnings project_options unifex::unifex)
