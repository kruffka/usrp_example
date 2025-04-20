cmake_minimum_required(VERSION 3.10)

project(usrp_example LANGUAGES C CXX)

include("cmake/FindGit.cmake")

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
  set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Release" "RelWithDebInfo" "Debug")
endif(NOT CMAKE_BUILD_TYPE)

if(NOT CMAKE_SYSTEM_PROCESSOR STREQUAL "x86_64")
    message(WARNING "TODO: May not compile for '${CMAKE_SYSTEM_PROCESSOR}'")
endif()

message("======================== Build for ${CMAKE_SYSTEM_PROCESSOR} ========================")

message(STATUS "Sources Version is ${GIT_VERSION}")
message(STATUS "CMAKE_SYSTEM: " ${CMAKE_SYSTEM})
message(STATUS "CMAKE_SYSTEM_PROCESSOR: " ${CMAKE_SYSTEM_PROCESSOR})
message(STATUS "CMAKE_C_COMPILER: " ${CMAKE_C_COMPILER})
message(STATUS "CMAKE_CXX_COMPILER: " ${CMAKE_CXX_COMPILER})
message(STATUS "CMAKE_BUILD_TYPE is " ${CMAKE_BUILD_TYPE})

set(USRP_LIB_SRC ${CMAKE_CURRENT_SOURCE_DIR}/usrp)
set(COMMON_LIB_SRC ${CMAKE_CURRENT_SOURCE_DIR}/common)
set(SIMDE_LIB_SRC ${CMAKE_CURRENT_SOURCE_DIR}/simde)

#
# Compile flags
#
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 11)


set(debugOpt "-ggdb2 -DMALLOC_CHECK_=3 -fno-delete-null-pointer-checks")
set(CMAKE_C_FLAGS_DEBUG "${debugOpt} -O0")
set(CMAKE_C_FLAGS_RELWITHDEBINFO "${debugOpt} -O2")
set(CMAKE_C_FLAGS_RELEASE "-O3")

set(commonOpts "-march=native -pipe -Wno-packed-bitfield-compat -funroll-loops -fPIC -Wall -fno-strict-aliasing -rdynamic \
                -DLV_HAVE_AVX2 -DLV_HAVE_AVX -DLV_HAVE_SSE -DLV_HAVE_AVX512 -D_GNU_SOURCE")

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${commonOpts} -std=gnu11 -Wmissing-parameter-type -Wempty-body \
    -Wold-style-declaration -Wunused-variable -Wunused-but-set-variable -Wunused-parameter \
    -Wunused-but-set-parameter -Wtype-limits")

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${commonOpts} -std=c++11")


include_directories(${CMAKE_CURRENT_SOURCE_DIR})
include_directories(${USRP_LIB_SRC})
include_directories(${COMMON_LIB_SRC})
include_directories(${SIMDE_LIB_SRC})

add_subdirectory(${COMMON_LIB_SRC})
add_subdirectory(${USRP_LIB_SRC})

add_executable(usrp_example usrp_main.c)
target_link_libraries(usrp_example common usrplib pthread)