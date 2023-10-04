set(INCLUDE_DIRS
    ${CMAKE_CURRENT_LIST_DIR}
)

include_directories(
    ${INCLUDE_DIRS}
)

add_subdirectory(example01)
add_subdirectory(example02)
add_subdirectory(example04)
add_subdirectory(example05)
add_subdirectory(example06)

if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND NOT CMAKE_CXX_COMPILER_ID MATCHES ".*Clang")
    add_subdirectory(example07)
endif()
