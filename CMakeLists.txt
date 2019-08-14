cmake_minimum_required(VERSION 3.2)

option (BUILD_QT "Make a Qt Demo" OFF)
option (BUILD_IMGUI "Make an imgui demo" ON)
option (BUILD_IMGUI_APP "Make the test app" ON)

option (ZEP_FEATURE_CPP_FILE_SYSTEM "Default File system enabled" ON)

set (CMAKE_CXX_STANDARD 14)
set (CMAKE_CXX_STANDARD_REQUIRED ON)

set (CMAKE_EXPORT_COMPILE_COMMANDS ON)

PROJECT (ZepDemo)

IF (ZEP_FEATURE_CPP_FILE_SYSTEM)
ADD_DEFINITIONS(-DZEP_FEATURE_CPP_FILE_SYSTEM)
ENDIF()

ADD_DEFINITIONS(-DZEP_USE_SDL)

MESSAGE(STATUS " CMakeLists: Zep")

SET_PROPERTY(GLOBAL PROPERTY USE_FOLDERS ON)

if (CMAKE_BUILD_TYPE MATCHES Coverage)
    set (CMAKE_BUILD_TYPE "Debug")
    set (PROJECT_COVERAGE ON)
endif()

# global needed variables
SET (APPLICATION_NAME "Zep")
SET (APPLICATION_VERSION_MAJOR "0")
SET (APPLICATION_VERSION_MINOR "1")
SET (APPLICATION_VERSION_PATCH "0")
SET (APPLICATION_VERSION "${APPLICATION_VERSION_MAJOR}.${APPLICATION_VERSION_MINOR}.${APPLICATION_VERSION_PATCH}")

if (BUILD_QT)
SET(CMAKE_AUTORCC ON)
SET(CMAKE_PREFIX_PATH $ENV{QT_INSTALL_LOCATION})
FIND_PACKAGE(Qt5 COMPONENTS Core Widgets Gui REQUIRED)
SET_PROPERTY(GLOBAL PROPERTY AUTOMOC_FOLDER Automoc)
SET_PROPERTY(GLOBAL PROPERTY AUTOGEN_TARGETS_FOLDER Automoc)
endif()

# Third party sources
SET(M3RDPARTY_INCLUDE "")

# Set all compiler flags 
INCLUDE(cmake/all.cmake)

# Functions for file copying
INCLUDE(m3rdparty/cmake/copy_files.cmake)

# config_app.h checks
# This makes a config_shared.h file which can be included for system settings
#  Must be after setting up the platforms
SET (ZEP_ROOT ${CMAKE_CURRENT_LIST_DIR})
CONFIGURE_FILE(${CMAKE_CURRENT_LIST_DIR}/cmake/config_app.h.cmake ${CMAKE_BINARY_DIR}/config_app.h)

INCLUDE(m3rdparty/list.cmake)
INCLUDE(src/list.cmake)

# Create the library - No depenencies on anything else in this file
ADD_LIBRARY(Zep STATIC ${ZEP_SOURCE} ${ZEP_INCLUDE})
TARGET_INCLUDE_DIRECTORIES(Zep 
PRIVATE
    src/mcommon
    src
    ${SRC_INCLUDE}
    ${CMAKE_BINARY_DIR}
PUBLIC
    include
    ) 

# Create the ImGUI zep library to help with building imgui apps 
IF (BUILD_IMGUI)
ADD_LIBRARY(ZepImGui STATIC ${ZEP_SOURCE_IMGUI})
TARGET_INCLUDE_DIRECTORIES(ZepImGui 
    PRIVATE 
        ${ZEP_INCLUDE_IMGUI}
        ${M3RDPARTY_DIR}/imgui
    PUBLIC include)

# Build the zep demo
IF (BUILD_IMGUI_APP)
IF(WIN32)
# Let windows know that the app is DPI Aware
SET(MANIFEST_SOURCE dpiawarescaling.manifest)
ENDIF()
ADD_EXECUTABLE (${PROJECT_NAME} WIN32 
    demo_imgui/main.cpp
    ${MANIFEST_SOURCE}
	${IMGUI_SOURCE}
    ${M3RDPARTY_DIR}/tfd/tinyfiledialogs.c
    ) # Win32 ignored on non-windows

TARGET_INCLUDE_DIRECTORIES(${PROJECT_NAME} 
    PRIVATE
        ${CMAKE_BINARY_DIR}
        ${M3RDPARTY_DIR}
        ${M3RDPARTY_DIR}/sdl/include
        ${M3RDPARTY_DIR}/tclap/include
        ${M3RDPARTY_DIR}/imgui
        ${M3RDPARTY_DIR}/tfd
        ${M3RDPARTY_DIR}/imgui/examples/libs/gl3w)
copy_existing_files(${PROJECT_NAME} m3rdparty/imgui/misc/fonts/ProggyClean.ttf ${CMAKE_CURRENT_BINARY_DIR}/$(Configuration) )
TARGET_LINK_LIBRARIES (${PROJECT_NAME} 
PRIVATE
    ZepImGui
    Zep
    ${PLATFORM_LINKLIBS}
    ${SDL_LINKLIBS})
ADD_DEPENDENCIES(${PROJECT_NAME} sdl2 ZepImGui Zep)
ENDIF()
ENDIF() # IMGUI

# Create the Qt version of the app
IF (BUILD_QT)
ADD_LIBRARY(ZepQt STATIC ${ZEP_SOURCE_QT})
SET(CMAKE_AUTOMOC ON)
SET(RESOURCE_FOLDER "" )
SET(RESOURCE_FILES "")
INCLUDE(demo_qt/list.cmake)

TARGET_INCLUDE_DIRECTORIES(ZepQt
PRIVATE
    ${ZEP_INCLUDE_QT}
    ${M3RDPARTY_DIR}
PUBLIC
    include)

ADD_EXECUTABLE (${PROJECT_NAME}-qt WIN32 ${DEMO_SOURCE_QT} ${RESOURCE_FILES}) # Win32 ignored on non-windows
TARGET_INCLUDE_DIRECTORIES(${PROJECT_NAME}-qt
PRIVATE
    ${ZEP_INCLUDE_QT}
    ${CMAKE_BINARY_DIR})
TARGET_LINK_LIBRARIES(${PROJECT_NAME}-qt PRIVATE Qt5::Core Qt5::Gui Qt5::Widgets ZepQt Zep ${PLATFORM_LINKLIBS})
TARGET_COMPILE_OPTIONS(${PROJECT_NAME}-qt PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/W3>) # Workaround Qt + MSVC 19 compile issue in release build.
ADD_DEPENDENCIES(${PROJECT_NAME} sdl2 ZepQt)
if(WIN32)
MESSAGE(STATUS "Copying required Qt libraries and binaries to output directory....")
# Run winddeployqt if it can be found, to ensure installed dependencies
find_program(WINDEPLOYQT_EXECUTABLE NAMES windeployqt HINTS ${QTDIR} ENV QTDIR PATH_SUFFIXES bin)
add_custom_command(TARGET ${PROJECT_NAME}-qt POST_BUILD
COMMAND ${WINDEPLOYQT_EXECUTABLE} $<TARGET_FILE:${PROJECT_NAME}-qt>)
ENDIF() # Win32
ENDIF() # Build QT

# Unit tests
# Require SDL/IMgui build to work
# Need to build app to build tests
IF (BUILD_IMGUI_APP)
SET(CMAKE_AUTOMOC OFF)
INCLUDE(tests/list.cmake)
enable_testing()
SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DGTEST_HAS_TR1_TUPLE=0")
SET (TEST_SOURCES
    ${M3RDPARTY_DIR}/googletest/googletest/src/gtest-all.cc
    ${TEST_SOURCES}
)
ADD_EXECUTABLE (unittests ${TEST_SOURCES} )
ADD_DEPENDENCIES(unittests sdl2 ZepImGui Zep)
TARGET_LINK_LIBRARIES (unittests PRIVATE ZepImGui Zep ${PLATFORM_LINKLIBS} ${CMAKE_THREAD_LIBS_INIT})
add_test(unittests unittests)
TARGET_INCLUDE_DIRECTORIES(unittests PRIVATE
    ${M3RDPARTY_DIR}/googletest/googletest/include
    ${M3RDPARTY_DIR}/googletest/googletest
    ${M3RDPARTY_DIR}/googletest/googlemock/include
    ${M3RDPARTY_DIR}/googletest/googlemock
    ${CMAKE_BINARY_DIR}
)
ENDIF()

SOURCE_GROUP (Zep\\common REGULAR_EXPRESSION "src/mcommon/.*")
SOURCE_GROUP (Zep\\common REGULAR_EXPRESSION "include/zep/mcommon/.*")
SOURCE_GROUP (Zep REGULAR_EXPRESSION "src/.*")
SOURCE_GROUP (Zep\\include REGULAR_EXPRESSION "include/zep/.*")
SOURCE_GROUP (Zep FILES ${DEMO_SOURCE_IMGUI})
SOURCE_GROUP (Zep FILES ${DEMO_SOURCE_QT})

SOURCE_GROUP(qt\\AutoMoc FILES ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}_automoc.cpp ) 
SOURCE_GROUP(qt\\AutoMoc REGULAR_EXPRESSION "(mocs_*|qrc_.*|QtAwesome.*)" ) 
