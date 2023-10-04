find_package(stdexec CONFIG REQUIRED)

add_executable(example01 main.cpp)
target_link_libraries(example01 PRIVATE project_warnings project_options STDEXEC::stdexec)
