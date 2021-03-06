get_filename_component(STOPWATCH_CMAKE_DIR "${CMAKE_CURRENT_LIST_FILE}" PATH)
include(CMakeFindDependencyMacro)

list(APPEND CMAKE_MODULE_PATH ${STOPWATCH_CMAKE_DIR})
find_dependency(PAPI REQUIRED)
list(REMOVE_AT CMAKE_MODULE_PATH -1)

if(NOT TARGET Stopwatch::Stopwatch)
    include("${STOPWATCH_CMAKE_DIR}/StopwatchTargets.cmake")
endif()

set(STOPWATCH_LIBRARIES Stopwatch::Stopwatch)
