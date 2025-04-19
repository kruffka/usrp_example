cmake_minimum_required(VERSION 3.10)

project(usrp_example)

include("cmake/FindGit.cmake")

set(USRP_LIB_SRC ${CMAKE_CURRENT_SOURCE_DIR}/usrp)
set(COMMON_LIB_SRC ${CMAKE_CURRENT_SOURCE_DIR}/common)

include_directories(${USRP_LIB_SRC})
include_directories(${COMMON_LIB_SRC})

add_subdirectory(${USRP_LIB_SRC})
add_subdirectory(${COMMON_LIB_SRC})

add_executable(usrp_example usrp_main.c)
target_link_libraries(usrp_example common usrplib)