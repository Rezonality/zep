cmake_minimum_required(VERSION 3.2)

message(STATUS "CMakeLists: Zep")
message(STATUS "Build Type: " ${CMAKE_BUILD_TYPE})

list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_LIST_DIR}/cmake)

# Global Options
option(BUILD_QT "Make Qt Library" OFF)
option(BUILD_IMGUI "Make Imgui Library" ON)
option(BUILD_DEMOS "Make the demo app" ON)
option(BUILD_TESTS "Make the tests" ON)
option(BUILD_EXTENSIONS "Make the extension library (required for demo)" OFF)
option(ZEP_FEATURE_CPP_FILE_SYSTEM "Default File system enabled" ON)

# Global Settings
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

set(gtest_force_shared_crt ON CACHE BOOL "Always use msvcrt.dll" FORCE)

set(CMAKE_DEBUG_POSTFIX "-debug")
set(CMAKE_RELWITHDEBINFO_POSTFIX "-reldbg")

project(Zep
    LANGUAGES CXX C
    VERSION 0.5.0
)

# Used Packages
include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

# Can I remove these now?
if (ZEP_FEATURE_CPP_FILE_SYSTEM)
add_definitions(-DZEP_FEATURE_CPP_FILE_SYSTEM)
endif()

if (BUILD_DEMOS)
SET(BUILD_EXTENSIONS ON)
endif()

# config_app.h checks
# This makes a config_shared.h file which can be included for system settings
#  Must be after setting up the platforms
set (ZEP_ROOT ${CMAKE_CURRENT_LIST_DIR})
set (M3RDPARTY_DIR ${CMAKE_CURRENT_LIST_DIR}/m3rdparty)

configure_file(${ZEP_ROOT}/cmake/config_app.h.cmake ${CMAKE_BINARY_DIR}/config_app.h)

# Set all compiler flags 
include(cmake/all.cmake)

# Functions for file copying
include(m3rdparty/cmake/copy_files.cmake)

# The main library
add_subdirectory(extensions)
add_subdirectory(src)
add_subdirectory(tests)
add_subdirectory(demos)

# Make the CMake bits that ensure find_package does the right thing
install(EXPORT zep-targets
    FILE zep-targets.cmake
    NAMESPACE Zep::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/zep
)

configure_package_config_file(
    ${ZEP_ROOT}/cmake/zep-config.cmake.in
    ${CMAKE_BINARY_DIR}/cmake/zep-config.cmake
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/zep
)

write_basic_package_version_file(
    ${CMAKE_BINARY_DIR}/cmake/zep-config-version.cmake
    VERSION 1.0
    COMPATIBILITY AnyNewerVersion
)

install(
    FILES
        ${CMAKE_BINARY_DIR}/cmake/zep-config.cmake
        ${CMAKE_BINARY_DIR}/cmake/zep-config-version.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/zep
)

include(${CMAKE_CURRENT_LIST_DIR}/cmake/cpack_installer.cmake)

