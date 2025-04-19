find_package(Git)


set(GIT_BRANCH "UNKNOWN")
set(GIT_COMMIT_HASH "UNKNOWN")
set(GIT_COMMIT_DATE "UNKNOWN")

if(GIT_EXECUTABLE)

  # Get the current working branch
  execute_process(
    COMMAND ${GIT_EXECUTABLE} rev-parse --abbrev-ref HEAD
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    OUTPUT_VARIABLE GIT_BRANCH
    OUTPUT_STRIP_TRAILING_WHITESPACE)

  # Get the latest abbreviated commit hash of the working branch
  execute_process(
    COMMAND ${GIT_EXECUTABLE} log -1 --format=%h
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    OUTPUT_VARIABLE GIT_COMMIT_HASH
    OUTPUT_STRIP_TRAILING_WHITESPACE)

  # Get the latest commit date of the working branch
  execute_process(
    COMMAND ${GIT_EXECUTABLE} log -1 --format=%cd
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    OUTPUT_VARIABLE GIT_COMMIT_DATE
    OUTPUT_STRIP_TRAILING_WHITESPACE)

endif()


set(GIT_VERSION "\"Branch: ${GIT_BRANCH} Abrev. Hash: ${GIT_COMMIT_HASH} Date: ${GIT_COMMIT_DATE}\"")
message(STATUS "Version is ${GIT_VERSION}")
add_definitions("-DGIT_VERSION=${GIT_VERSION}")
