# Compiler flags
if(CMAKE_COMPILER_IS_GNUCXX)
  include(${PROJECT_SOURCE_DIR}/cmake/g++.cmake)
elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  include(${PROJECT_SOURCE_DIR}/cmake/clang++.cmake)
elseif(MSVC)
  include(${PROJECT_SOURCE_DIR}/cmake/msvc.cmake)
else()
  message(WARNING "Unknown compiler, not setting flags")
endif()

# Ensure Debug Flags
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -D_DEBUG -DDEBUG")

message(STATUS "System: ${CMAKE_SYSTEM}")
message(STATUS "Compiler: ${CMAKE_CXX_COMPILER_ID}")
message(STATUS "Flags: ${CMAKE_CXX_FLAGS}")
message(STATUS "Arch: ${PROCESSOR_ARCH}")

# System flags
if ("${CMAKE_SYSTEM_NAME}" STREQUAL "Windows")
    message(STATUS "TARGET_PC")
    SET(TARGET_PC 1)
    SET(PROJECT_CPP_FILESYSTEM 1)
    INCLUDE(${PROJECT_SOURCE_DIR}/cmake/pc.cmake)
endif()

if("${CMAKE_SYSTEM_NAME}" STREQUAL "Darwin")
    message(STATUS "TARGET_MAC")
    SET(TARGET_MAC 1)
    INCLUDE(${PROJECT_SOURCE_DIR}/cmake/mac.cmake)
endif()

if("${CMAKE_SYSTEM_NAME}" STREQUAL "Linux")
    message(STATUS "TARGET_LINUX")
    SET(TARGET_LINUX 1)
    SET(PROJECT_CPP_FILESYSTEM 1)
    INCLUDE(${PROJECT_SOURCE_DIR}/cmake/linux.cmake)
endif()

