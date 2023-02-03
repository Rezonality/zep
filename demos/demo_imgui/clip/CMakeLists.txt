# Clip Library
# Copyright (C) 2015-2018 David Capello

cmake_minimum_required(VERSION 3.1.2)

project(clip)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(CMAKE_CXX_COMPILER_ID STREQUAL "AppleClang")
  # Use libc++ explicitly so we can compile for
  # CMAKE_OSX_DEPLOYMENT_TARGET=10.7 or 10.8
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=libc++")
endif()

option(CLIP_EXAMPLES "Compile clip examples" on)
option(CLIP_TESTS "Compile clip tests" on)
if(UNIX AND NOT APPLE)
  option(CLIP_X11_WITH_PNG "Compile with libpng to support copy/paste image in png format" on)
endif()

include_directories(.)
set(CLIP_SOURCES clip.cpp image.cpp)

if(WIN32)
  add_definitions(-D_SCL_SECURE_NO_WARNINGS)
  list(APPEND CLIP_SOURCES clip_win.cpp)
elseif(APPLE)
  add_definitions(-fobjc-arc)

  find_library(COCOA_LIBRARY Cocoa)
  if(COCOA_LIBRARY)
    list(APPEND CLIP_SOURCES clip_osx.mm)
  else()
    list(APPEND CLIP_SOURCES clip_none.cpp)
  endif()
elseif(UNIX)
  include(CheckIncludeFiles)
  check_include_files(xcb/xcb.h HAVE_XCB_XLIB_H)

  if(HAVE_XCB_XLIB_H)
    add_definitions(-DHAVE_XCB_XLIB_H)

    if(CLIP_X11_WITH_PNG)
      check_include_files(png.h HAVE_PNG_H)
      if(CLIP_X11_PNG_LIBRARY)
        set(PNG_LIBRARY ${CLIP_X11_PNG_LIBRARY})
      else()
        find_library(PNG_LIBRARY png)
      endif()
      if(HAVE_PNG_H AND PNG_LIBRARY)
        add_definitions(-DHAVE_PNG_H)
      endif()
    endif()

    list(APPEND CLIP_SOURCES clip_x11.cpp)
  else()
    list(APPEND CLIP_SOURCES clip_none.cpp)
  endif()
else()
  list(APPEND CLIP_SOURCES clip_none.cpp)
endif()

add_library(clip ${CLIP_SOURCES})

if(APPLE)
  if(COCOA_LIBRARY)
    target_link_libraries(clip ${COCOA_LIBRARY})
  endif()
elseif(UNIX)
  if(HAVE_XCB_XLIB_H)
    target_link_libraries(clip xcb pthread)
    if(CLIP_X11_WITH_PNG AND HAVE_PNG_H AND PNG_LIBRARY)
      target_link_libraries(clip ${PNG_LIBRARY})
    endif()
  endif()
endif()

if(CLIP_EXAMPLES)
  add_subdirectory(examples)
endif()

if(CLIP_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()
