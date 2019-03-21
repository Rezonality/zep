cmake_minimum_required(VERSION 3.1)

set (CMAKE_CXX_STANDARD 17)
set (CMAKE_EXPORT_COMPILE_COMMANDS ON)

PROJECT (Jorvik)

MESSAGE(STATUS " CMakeLists: Jorvik")

# Options
option (PROJECT_UNITTESTS "Build Unit Tests" ON)
option (PROJECT_JORVIK "Build Jorvik" ON)
option (PROJECT_DEVICE_DX12 "Support DX12" ON)
option (PROJECT_DEVICE_VULKAN "Support Vulkan" ON)

# Docs not added for functions yet, but you can generate them
option (PROJECT_DOCS "Generate documentation" OFF)

SET_PROPERTY(GLOBAL PROPERTY USE_FOLDERS ON)

if (CMAKE_BUILD_TYPE MATCHES Coverage)
    set (CMAKE_BUILD_TYPE "Debug")
    set (PROJECT_COVERAGE ON)
    set (PROJECT_UNITTESTS ON)
endif()

# global needed variables
SET (APPLICATION_NAME "Jorvik")
SET (APPLICATION_VERSION_MAJOR "0")
SET (APPLICATION_VERSION_MINOR "1")
SET (APPLICATION_VERSION_PATCH "0")
SET (APPLICATION_VERSION "${APPLICATION_VERSION_MAJOR}.${APPLICATION_VERSION_MINOR}.${APPLICATION_VERSION_PATCH}")

# Gather our lists of source code
SET(COMMON_SOURCES "")

# Core stuff
SET(JORVIK_SOURCES "")
SET(JORVIK_INCLUDE "")

# Third party sources
SET(M3RDPARTY_SOURCE "")
SET(M3RDPARTY_INCLUDE "")

# Tests
SET(TEST_SOURCES "")

# Link libraries
SET(PLATFORM_LINKLIBS "")
SET(PLATFORM_LINKLIBS_DEBUG "")
SET(PLATFORM_LINKLIBS_RELEASE "")

# Set all compiler flags 
INCLUDE(cmake/all.cmake)

# Functions for file copying
INCLUDE(m3rdparty/cmake/copy_files.cmake)

set (JORVIK_ROOT ${CMAKE_CURRENT_LIST_DIR})

# config_app.h checks
# This makes a config_shared.h file which can be included for system settings
#  Must be after setting up the platforms
CONFIGURE_FILE(${CMAKE_CURRENT_LIST_DIR}/cmake/config_app.h.cmake ${CMAKE_BINARY_DIR}/config_app.h)

INCLUDE(m3rdparty/list.cmake)
INCLUDE(utils/list.cmake)

SET(BUILD_IMGUI TRUE)
#INCLUDE(m3rdparty/zep/src/list.cmake)

INCLUDE_DIRECTORIES(
    m3rdparty
    utils
    ${COMMON_INCLUDE}
    ${M3RDPARTY_INCLUDE}
    ${CMAKE_BINARY_DIR}
    .
    ) 

# Create the library - No depenencies on anything else in this file
ADD_LIBRARY(utils 
	${COMMON_SOURCES}
	${M3RDPARTY_SOURCE}
)

# Create the app ***
IF (PROJECT_JORVIK)
SET(JORVIK_SOURCES "")
SET(JORVIK_INCLUDES "")
INCLUDE(jorvik/list.cmake)

INCLUDE_DIRECTORIES(${JORVIK_INCLUDES})
ADD_EXECUTABLE (${PROJECT_NAME} WIN32 ${JORVIK_SOURCES}) # Win32 ignored on non-windows
ADD_DEPENDENCIES(${PROJECT_NAME} sdl2)
TARGET_LINK_LIBRARIES (${PROJECT_NAME} utils ${PLATFORM_LINKLIBS})
copy_existing_files(${PROJECT_NAME} logger.conf ${CMAKE_CURRENT_BINARY_DIR}/$(Configuration) )
copy_existing_files(${PROJECT_NAME} ${JORVIK_ROOT}/m3rdparty/pix/bin/WinPixEventRuntime.dll ${CMAKE_CURRENT_BINARY_DIR}/$(Configuration) )
ENDIF()
# End Create

# Create unit tests
IF (PROJECT_UNITTESTS)
INCLUDE(tests/list.cmake)
enable_testing()
SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DGTEST_HAS_TR1_TUPLE=0")
SET (TEST_SOURCES
    ${M3RDPARTY_DIR}/googletest/googletest/src/gtest-all.cc
    ${TEST_SOURCES}
)
ADD_EXECUTABLE (unittests ${TEST_SOURCES} )
ADD_DEPENDENCIES(unittests sdl2)

TARGET_LINK_LIBRARIES (unittests utils ${PLATFORM_LINKLIBS} ${CMAKE_THREAD_LIBS_INIT})
add_test(unittests unittests)
INCLUDE_DIRECTORIES(
    ${M3RDPARTY_DIR}/googletest/googletest/include
    ${M3RDPARTY_DIR}/googletest/googletest
    ${M3RDPARTY_DIR}/googletest/googlemock/include
    ${M3RDPARTY_DIR}/googletest/googlemock
)
ENDIF()
#End Create Unit Tests

# Begin Docs
if(PROJECT_DOCS)
  message(STATUS "Generate documentation")

  find_package(Doxygen)
  if(DOXYGEN_FOUND)
    configure_file(
      ${PROJECT_SOURCE_DIR}/cmake/Doxyfile.in
      ${CMAKE_BINARY_DIR}/Doxyfile
    )
    add_custom_target(gen-doc
      ${DOXYGEN_EXECUTABLE} ${CMAKE_BINARY_DIR}/Doxyfile
      WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
      COMMENT "Generating documentation"
    )
  else()
    message(WARNING "Doxygen not found, skipping documentation generation")
  endif()
endif()
# End Docs


SOURCE_GROUP(2D REGULAR_EXPRESSION "2d")
SOURCE_GROUP(3D REGULAR_EXPRESSION "3d")
SOURCE_GROUP(3D\\Geometry REGULAR_EXPRESSION "(3d)+.*geometry*")
SOURCE_GROUP(3D\\Camera REGULAR_EXPRESSION "(3d)+.*camera*")
SOURCE_GROUP(Device REGULAR_EXPRESSION "(3d)+.*device*")
SOURCE_GROUP(3D\\UI REGULAR_EXPRESSION "(3d)+.*ui*")
SOURCE_GROUP(3D\\Scene REGULAR_EXPRESSION "(3d)+.*scene*")
SOURCE_GROUP(run_tree\\Shaders\\GL REGULAR_EXPRESSION "(fragmentshader|vertexshader)+$")
SOURCE_GROUP(run_tree\\Shaders\\DX REGULAR_EXPRESSION "(mhlsl)+$")
SOURCE_GROUP(run_tree\\Shaders\\GLSLANG REGULAR_EXPRESSION "(vert|tesc|tese|geom|frag|comp|conf)+$")
SOURCE_GROUP(Entities REGULAR_EXPRESSION "(entities)+")
SOURCE_GROUP(Systems REGULAR_EXPRESSION "(systems)+")
SOURCE_GROUP(Jorvik\\Visual\\DX12 REGULAR_EXPRESSION "jorvik/visual/dx12.*")
SOURCE_GROUP(Jorvik\\Visual\\Vulkan REGULAR_EXPRESSION "jorvik/visual/vulkan.*")
SOURCE_GROUP(Jorvik\\Visual REGULAR_EXPRESSION "jorvik/visual/.*")
SOURCE_GROUP(Jorvik REGULAR_EXPRESSION "jorvik/.*")

SOURCE_GROUP(newton REGULAR_EXPRESSION "utils/newton.*")
SOURCE_GROUP(schema REGULAR_EXPRESSION "utils/schema.*")
SOURCE_GROUP(ui REGULAR_EXPRESSION "utils/ui.*")
SOURCE_GROUP(animation REGULAR_EXPRESSION "utils/animation.*")
SOURCE_GROUP(file REGULAR_EXPRESSION "utils/file.*")
SOURCE_GROUP(string REGULAR_EXPRESSION "utils/string.*")
SOURCE_GROUP(math REGULAR_EXPRESSION "utils/math.*")

SOURCE_GROUP(tests REGULAR_EXPRESSION ".*.test.(cpp|h)+")

