cmake_minimum_required(VERSION 3.1.0)
project(cpptoml)

set(cpptoml_VERSION 0.4.0)
set(cpptoml_TOML_VERSION 0.4.0)

include(CheckCXXCompilerFlag)
include(CheckCXXSourceCompiles)
include(CheckCXXSourceRuns)
include(CMakePushCheckState)

cmake_push_check_state()

option(ENABLE_LIBCXX "Use libc++ for the C++ standard library" ON)
option(CPPTOML_BUILD_EXAMPLES "Build examples" ON)

set(CMAKE_EXPORT_COMPILE_COMMANDS 1)

list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/deps/meta-cmake)

if(UNIX OR MINGW)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -pedantic")

  if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
    if(CMAKE_GENERATOR STREQUAL "Ninja")
      set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fcolor-diagnostics")
    endif()

    if(ENABLE_LIBCXX)
      find_package(LIBCXX REQUIRED)
      set_libcxx_required_flags()
    endif()
  endif()
endif()

set(CMAKE_REQUIRED_FLAGS "${CMAKE_REQUIRED_FLAGS} ${CMAKE_CXX11_STANDARD_COMPILE_OPTION}")

cmake_pop_check_state()

add_library(cpptoml INTERFACE)

target_include_directories(cpptoml INTERFACE
  $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>)

if (LIBDL_LIBRARY)
  target_link_libraries(cpptoml INTERFACE ${LIBDL_LIBRARY})
endif()

if (CXXABI_LIBRARY)
  target_link_libraries(cpptoml INTERFACE ${CXXABI_LIBRARY})
endif()

if (CPPTOML_BUILD_EXAMPLES)
  set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
  add_subdirectory(examples)
endif()

find_package(Doxygen)
if(DOXYGEN_FOUND AND NOT TARGET doc)
  configure_file(${CMAKE_CURRENT_SOURCE_DIR}/cpptoml.doxygen.in
    ${CMAKE_CURRENT_BINARY_DIR}/cpptoml.doxygen @ONLY)
  add_custom_target(doc
    ${DOXYGEN_EXECUTABLE}
    ${CMAKE_CURRENT_BINARY_DIR}/cpptoml.doxygen
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
endif()

include(CMakePackageConfigHelpers)
write_basic_package_version_file(
  ${CMAKE_CURRENT_BINARY_DIR}/cpptoml/cpptomlConfigVersion.cmake
  VERSION ${cpptoml_VERSION}
  COMPATIBILITY SameMajorVersion)
configure_file(cmake/cpptomlConfig.cmake.in
               ${CMAKE_CURRENT_BINARY_DIR}/cpptoml/cpptomlConfig.cmake
               COPYONLY)

install(TARGETS cpptoml
        EXPORT cpptoml-exports)
install(FILES include/cpptoml.h
        DESTINATION include)
install(EXPORT cpptoml-exports
        FILE cpptomlTargets.cmake
        DESTINATION lib/cmake/cpptoml)
install(FILES
          ${CMAKE_CURRENT_BINARY_DIR}/cpptoml/cpptomlConfigVersion.cmake
          ${CMAKE_CURRENT_BINARY_DIR}/cpptoml/cpptomlConfig.cmake
        DESTINATION
          lib/cmake/cpptoml)

export(EXPORT cpptoml-exports
       FILE ${CMAKE_CURRENT_BINARY_DIR}/cpptoml/cpptomlTargets.cmake)
export(PACKAGE cpptoml)
