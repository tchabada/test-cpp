cmake_minimum_required(VERSION 3.15)

include(cmake/Vcpkg.cmake)

# Set global property (all targets are impacted)
set_property(GLOBAL PROPERTY RULE_LAUNCH_COMPILE "${CMAKE_COMMAND} -E time")

# Set the project name to your project name, my project isn't very descriptive
project(myproject CXX)
include(cmake/StandardProjectSettings.cmake)

# Link this 'library' to set the compile-time options requested
add_library(project_options INTERFACE)

# Link this 'library' to use the warnings specified in CompilerWarnings.cmake
add_library(project_warnings INTERFACE)

# standard compiler warnings
include(cmake/CompilerWarnings.cmake)
set_project_warnings(project_warnings)

# sanitizer options if supported by compiler
include(cmake/Sanitizers.cmake)
enable_sanitizers(project_options)

add_subdirectory(src)

enable_testing()

set(TESTS_INCLUDE_DIRS
    ${PROJECT_SOURCE_DIR}/src
)

include_directories(
    ${TESTS_INCLUDE_DIRS}
)

add_subdirectory(test)
