find_package(Boost REQUIRED)

if(NOT Boost_FOUND)
    message(FATAL_ERROR "Boost library not found")
endif()

find_package(UHD REQUIRED UHD)

if(NOT UHD_FOUND)
    message(FATAL_ERROR "UHD library not found")
endif()

add_library(usrplib STATIC usrp_lib.cc)

target_link_libraries(usrplib PRIVATE ${UHD_LIBRARIES} -lboost_log -lboost_thread -lpthread common)
target_include_directories(usrplib PRIVATE ${UHD_INCLUDE_DIRS} ${Boost_INCLUDE_DIR})
