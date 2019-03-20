#
# clang++ options
#

MESSAGE(STATUS "Clang++.cmake")
# Compatible with g++
include(${PROJECT_SOURCE_DIR}/cmake/g++.cmake)

# Dependencies required for linking executables
#if(${CMAKE_SYSTEM_NAME} STREQUAL "Linux")
  #list(APPEND SAL_DEP_LIBS atomic)
#endif()
