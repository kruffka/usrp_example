file(GLOB SRC_FILES ${CMAKE_CURRENT_SOURCE_DIR}/*.c)

add_library(common STATIC ${SRC_FILES})

