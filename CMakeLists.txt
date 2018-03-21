cmake_minimum_required(VERSION 3.2)

option (BUILD_QT "Make a Qt Demo" OFF)
option (BUILD_IMGUI "Make an imgui demo" ON)

set (CMAKE_CXX_STANDARD 14)
set (CMAKE_EXPORT_COMPILE_COMMANDS ON)

PROJECT (ZepDemo)

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
SET(M3RDPARTY_SOURCE "")
SET(M3RDPARTY_INCLUDE "")

# Set all compiler flags 
INCLUDE(cmake/all.cmake)

# Functions for file copying
INCLUDE(m3rdparty/cmake/copy_files.cmake)

# config_app.h checks
# This makes a config_shared.h file which can be included for system settings
#  Must be after setting up the platforms
CONFIGURE_FILE(${CMAKE_CURRENT_LIST_DIR}/cmake/config_app.h.cmake ${CMAKE_BINARY_DIR}/config_app.h)

INCLUDE(m3rdparty/list.cmake)
INCLUDE(src/list.cmake)

INCLUDE_DIRECTORIES(
    m3rdparty
    ${M3RDPARTY_INCLUDE}
    ${SRC_INCLUDE}
    ${CMAKE_BINARY_DIR}
    ${APP_ZEP_INCLUDE}
    ./
    ) 

# Create the library - No depenencies on anything else in this file
ADD_LIBRARY(Zep STATIC ${ZEP_SOURCE} ${ZEP_INCLUDE})

# Create the ImGUI version of the app
IF (BUILD_IMGUI)
INCLUDE_DIRECTORIES(${ZEP_INCLUDE_IMGUI})
IF (OPENGL_FOUND OR TARGET_PC)
SET(DEMO_SOURCE_IMGUI demo_imgui/main.cpp)

ADD_EXECUTABLE (${PROJECT_NAME} WIN32 
    ${DEMO_SOURCE_IMGUI}
	${ZEP_SOURCE_IMGUI}
	${IMGUI_SOURCE}
    ${M3RDPARTY_SOURCE}) # Win32 ignored on non-windows

copy_existing_files(${PROJECT_NAME} m3rdparty/imgui/extra_fonts/ProggyClean.ttf ${CMAKE_CURRENT_BINARY_DIR}/$(Configuration) )
TARGET_LINK_LIBRARIES (${PROJECT_NAME} Zep ${PLATFORM_LINKLIBS} ${SDL_LINKLIBS})
IF (BUILD_QT)
TARGET_LINK_LIBRARIES(${PROJECT_NAME} Qt5::Core Qt5::Widgets Zep ${PLATFORM_LINKLIBS})
ENDIF()
ADD_DEPENDENCIES(${PROJECT_NAME} sdl2)
ENDIF() # GL or PC
ENDIF() # IMGUI

# Create the Qt version of the app
IF (BUILD_QT)
SET(CMAKE_AUTOMOC ON)
SET(RESOURCE_FOLDER "" )
SET(RESOURCE_FILES "")
INCLUDE(demo_qt/list.cmake)
INCLUDE_DIRECTORIES(${ZEP_INCLUDE_QT})
ADD_EXECUTABLE (${PROJECT_NAME}-qt WIN32 ${DEMO_SOURCE_QT} ${ZEP_SOURCE_QT} ${M3RDPARTY_SOURCE} ${RESOURCE_FILES}) # Win32 ignored on non-windows
TARGET_LINK_LIBRARIES(${PROJECT_NAME}-qt Qt5::Core Qt5::Gui Qt5::Widgets Zep ${PLATFORM_LINKLIBS})
if(WIN32)
message(INFO "Copying required Qt libraries and binaries to output directory....")
# Run winddeployqt if it can be found, to ensure installed dependencies
find_program(WINDEPLOYQT_EXECUTABLE NAMES windeployqt HINTS ${QTDIR} ENV QTDIR PATH_SUFFIXES bin)
add_custom_command(TARGET ${PROJECT_NAME}-qt POST_BUILD
COMMAND ${WINDEPLOYQT_EXECUTABLE} $<TARGET_FILE:${PROJECT_NAME}-qt>)
ENDIF() # Win32
ENDIF() # Build QT

# Unit tests
# Require SDL/IMgui build to work
IF (BUILD_IMGUI)
SET(CMAKE_AUTOMOC OFF)
INCLUDE(tests/list.cmake)
enable_testing()
SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DGTEST_HAS_TR1_TUPLE=0")
SET (TEST_SOURCES
    ${M3RDPARTY_DIR}/googletest/googletest/src/gtest-all.cc
    ${TEST_SOURCES}
)
ADD_EXECUTABLE (unittests ${TEST_SOURCES} )
ADD_DEPENDENCIES(unittests sdl2)
TARGET_LINK_LIBRARIES (unittests Zep ${PLATFORM_LINKLIBS} ${CMAKE_THREAD_LIBS_INIT})
add_test(unittests unittests)
INCLUDE_DIRECTORIES(
    ${M3RDPARTY_DIR}/googletest/googletest/include
    ${M3RDPARTY_DIR}/googletest/googletest
    ${M3RDPARTY_DIR}/googletest/googlemock/include
    ${M3RDPARTY_DIR}/googletest/googlemock
)
ENDIF()

SOURCE_GROUP (Zep REGULAR_EXPRESSION "src/.*")
SOURCE_GROUP (Zep FILES ${DEMO_SOURCE_IMGUI})
SOURCE_GROUP (Zep FILES ${DEMO_SOURCE_QT})

SOURCE_GROUP(qt\\AutoMoc FILES ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}_automoc.cpp ) 
SOURCE_GROUP(qt\\AutoMoc REGULAR_EXPRESSION "(mocs_*|qrc_.*|QtAwesome.*)" ) 
