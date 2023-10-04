find_package(stdexec CONFIG REQUIRED)

add_executable(04 main.cpp)
target_link_libraries(04 PRIVATE project_warnings project_options STDEXEC::stdexec)
