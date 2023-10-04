find_package(unifex CONFIG REQUIRED)

add_executable(05 main.cpp)
target_link_libraries(05 PRIVATE project_warnings project_options unifex::unifex)
