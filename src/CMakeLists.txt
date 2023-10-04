set(INCLUDE_DIRS
    ${CMAKE_CURRENT_LIST_DIR}
)

include_directories(
    ${INCLUDE_DIRS}
)

add_subdirectory(01)
add_subdirectory(02)
add_subdirectory(04)
add_subdirectory(05)
add_subdirectory(06)

if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND NOT CMAKE_CXX_COMPILER_ID MATCHES ".*Clang")
    add_subdirectory(07)
endif()
