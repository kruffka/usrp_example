# find_package(Boost REQUIRED)
find_package(UHD REQUIRED UHD)

add_library(usrplib STATIC usrp_lib.cc)

target_link_libraries(usrplib PRIVATE ${UHD_LIBRARIES})
target_include_directories(usrplib PRIVATE ${UHD_INCLUDE_DIRS})
