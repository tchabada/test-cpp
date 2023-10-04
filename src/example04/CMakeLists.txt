find_package(stdexec CONFIG REQUIRED)

add_executable(example04 main.cpp)
target_link_libraries(example04 PRIVATE project_warnings project_options STDEXEC::stdexec)
