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

message(STATUS "System: ${CMAKE_SYSTEM}")
message(STATUS "Compiler: ${CMAKE_CXX_COMPILER_ID}")
message(STATUS "Flags: ${CMAKE_CXX_FLAGS}")
message(STATUS "Arch: ${PROCESSOR_ARCH}")

if(CMAKE_SIZEOF_VOID_P EQUAL 8)
  set(ARCH_64 TRUE)
  set(PROCESSOR_ARCH "x64")
else()
  set(ARCH_64 FALSE)
  set(PROCESSOR_ARCH "x86")
endif()

# System flags
if ("${CMAKE_SYSTEM_NAME}" STREQUAL "Windows")
    message(STATUS "TARGET_PC")
    set(TARGET_PC 1)
    include(${PROJECT_SOURCE_DIR}/cmake/pc.cmake)
endif()

if("${CMAKE_SYSTEM_NAME}" STREQUAL "Darwin")
    message(STATUS "TARGET_MAC")
    SET(TARGET_MAC 1)
    INCLUDE(${PROJECT_SOURCE_DIR}/cmake/mac.cmake)
endif()

if("${CMAKE_SYSTEM_NAME}" STREQUAL "Linux")
    message(STATUS "TARGET_LINUX")
    SET(TARGET_LINUX 1)
    INCLUDE(${PROJECT_SOURCE_DIR}/cmake/linux.cmake)
endif()
	
# global needed variables
SET (APPLICATION_NAME ${PROJECT_NAME})
SET (APPLICATION_VERSION_MAJOR "0")
SET (APPLICATION_VERSION_MINOR "1")
SET (APPLICATION_VERSION_PATCH "0")
SET (APPLICATION_VERSION "${APPLICATION_VERSION_MAJOR}.${APPLICATION_VERSION_MINOR}.${APPLICATION_VERSION_PATCH}")

