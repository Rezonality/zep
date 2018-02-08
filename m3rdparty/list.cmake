# ImGui

IF (BUILD_IMGUI)
SET (IMGUI_SOURCE 
    m3rdparty/imgui/examples/sdl_opengl3_example/imgui_impl_sdl_gl3.cpp
    m3rdparty/imgui/examples/libs/gl3w/GL/gl3w.c
    m3rdparty/imgui/imgui_demo.cpp
    m3rdparty/imgui/imgui_draw.cpp
    m3rdparty/imgui/imgui.cpp)

SET (IMGUI_INCLUDE 
    m3rdparty/imgui/imgui.h
    m3rdparty/imgui/imgui_dock.h
    m3rdparty/imgui/imgui_orient.h)

LIST(APPEND M3RDPARTY_INCLUDE
    m3rdparty/imgui
    m3rdparty/imgui/examples/libs/gl3w
    )
ENDIF()

LIST(APPEND M3RDPARTY_INCLUDE
    m3rdparty
    ${CMAKE_BINARY_DIR}
    m3rdparty/threadpool
    )

SET (M3RDPARTY_DIR ${CMAKE_CURRENT_LIST_DIR})

IF (BUILD_IMGUI)
INCLUDE(ExternalProject)
ExternalProject_Add(
  sdl2
  PREFIX "m3rdparty"
  CMAKE_ARGS -DCMAKE_DEBUG_POSTFIX='' -DCMAKE_STATIC=ON
  SOURCE_DIR "${M3RDPARTY_DIR}/sdl"
  TEST_COMMAND ""
  INSTALL_COMMAND "" 
  INSTALL_DIR ""
)

LIST(APPEND M3RDPARTY_INCLUDE
    m3rdparty/sdl/include
    m3rdparty/sdl
    )

IF (TARGET_PC)
SET(SDL_LINKLIBS 
    SDL2-static
    SDL2main
)
ENDIF()
IF (TARGET_MAC)
SET(SDL_LINKLIBS 
    sdl2-2.0
    sdl2main
)
ENDIF()
IF (TARGET_LINUX)
SET(SDL_LINKLIBS 
    SDL2-2.0
    SDL2main
)
ENDIF()

LINK_DIRECTORIES(${CMAKE_BINARY_DIR}/m3rdparty/src/sdl2-build)
ENDIF() # ImGui


SOURCE_GROUP ("m3rdparty\\imgui" REGULAR_EXPRESSION "(imgui.cpp|imgui_)+.*")
SOURCE_GROUP ("m3rdparty\\gl" REGULAR_EXPRESSION "(gl3w)+.*")

CONFIGURE_FILE(${CMAKE_CURRENT_LIST_DIR}/cmake/config_shared.h.cmake ${CMAKE_BINARY_DIR}/config_shared.h)
