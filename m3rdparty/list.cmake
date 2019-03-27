option (PROJECT_M3RDPARTY_ZIP "Compile Zip decompression" OFF)
option (PROJECT_CPP_FILESYSTEM "Use CPP 14 Filesystem - experimental" ON)
option (PROJECT_SHADERTOOLS "Build SpirV, glslang" ON)

# Zip
if (PROJECT_M3RDPARTY_ZIP)
    file(GLOB ZIP_SOURCE 
        m3rdparty/ziplib/Source/ZipLib/*.h
        m3rdparty/ziplib/Source/ZipLib/*.cpp
        m3rdparty/ziplib/Source/ZipLib/extlibs/bzip2/*.c
        m3rdparty/ziplib/Source/ZipLib/extlibs/zlib/*.c
        )

    IF (TARGET_LINUX)
        file(GLOB ZIP_SOURCE_LINUX 
            m3rdparty/ziplib/Source/ZipLib/extlibs/lzma/unix/*.c)
        set (ZIP_SOURCE ${ZIP_SOURCE} ${ZIP_SOURCE_LINUX})
    ENDIF()

    IF (TARGET_PC)
        file(GLOB ZIP_SOURCE_PC 
            m3rdparty/ziplib/Source/ZipLib/extlibs/lzma/*.c)
        set (ZIP_SOURCE ${ZIP_SOURCE} ${ZIP_SOURCE_PC})
    ENDIF()
ENDIF()

# GLM
file(GLOB GLM_SOURCE
    m3rdparty/glm/glm/*.hpp)

# File watcher
SET (FILEWATCHER_SOURCE
    m3rdparty/filewatcher/FileWatcher.cpp
    m3rdparty/filewatcher/FileWatcher.h)

IF (TARGET_PC)
SET (FILEWATCHER_SOURCE
    ${FILEWATCHER_SOURCE}
    m3rdparty/filewatcher/FileWatcherWin32.cpp
    )
ELSEIF(TARGET_LINUX)
SET (FILEWATCHER_SOURCE
    ${FILEWATCHER_SOURCE}
    m3rdparty/filewatcher/FileWatcherLinux.cpp
    )
ENDIF()

# MPC Parser
SET (MPC_SOURCE
    m3rdparty/mpc/mpc.c)

SET (MPC_INCLUDE 
    m3rdparty/mpc/mpc.h)

# GL3
IF (PROJECT_DEVICE_GL)
    SET (GL_SOURCE 
        m3rdparty/GL/gl3w.c)
    LIST(APPEND M3RDPARTY_INCLUDE m3rdparty/GL)
ENDIF()

# Json
LIST(APPEND M3RDPARTY_INCLUDE m3rdparty/json/include)

# ImGui
SET (IMGUI_SOURCE 
    m3rdparty/imgui/imgui_widgets.cpp
    m3rdparty/imgui/imgui_demo.cpp
    m3rdparty/imgui/imgui_draw.cpp
    m3rdparty/imgui/imgui.cpp)

IF (PROJECT_FREETYPE)
    SET (IMGUI_SOURCE ${IMGUI_SOURCE} m3rdparty/imgui/imgui_freetype.cpp)
    LIST(APPEND M3RDPARTY_INCLUDE m3rdparty/freetype/include)
ENDIF()

IF (PROJECT_DEVICE_DX12)
    SET(DX12_MINIENGINE_SOURCES
        #m3rdparty/miniengine/ForwardPlusLighting.cpp
        )

    SET (DX12_PIXEL_SHADERS
        #m3rdparty/miniengine/Core/Shaders/BicubicHorizontalUpsamplePS.hlsl
        )

    SET (DX12_VERTEX_SHADERS
        #m3rdparty/miniengine/Core/Shaders/ParticleNoSortVS.hlsl
        )

    SET (DX12_COMPUTE_SHADERS
        #m3rdparty/miniengine/Core/Shaders/AdaptExposureCS.hlsl 
        )

    SET(TARGET_BIN ${CMAKE_BINARY_DIR})

    foreach(SHADER ${DX12_COMPUTE_SHADERS})
        get_filename_component(NAMETXT ${SHADER} NAME_WE)	
        set_source_files_properties(${SHADER} PROPERTIES
            VS_SHADER_OUTPUT_HEADER_FILE "${TARGET_BIN}/CompiledShaders/${NAMETXT}.h"
            VS_SHADER_OBJECT_FILE_NAME "${TARGET_BIN}/Shaders/${NAMETXT}.cso"
            VS_SHADER_VARIABLE_NAME "g_p${NAMETXT}")
    endforeach()

    foreach(SHADER ${DX12_VERTEX_SHADERS})
        get_filename_component(NAMETXT ${SHADER} NAME_WE)	
        set_source_files_properties(${SHADER} PROPERTIES
            VS_SHADER_OUTPUT_HEADER_FILE "${TARGET_BIN}/CompiledShaders/${NAMETXT}.h"
            VS_SHADER_OBJECT_FILE_NAME "${TARGET_BIN}/Shaders/${NAMETXT}.cso"
            VS_SHADER_VARIABLE_NAME "g_p${NAMETXT}")
    endforeach()

    foreach(SHADER ${DX12_PIXEL_SHADERS})
        get_filename_component(NAMETXT ${SHADER} NAME_WE)	
        set_source_files_properties(${SHADER} PROPERTIES 
            VS_SHADER_OUTPUT_HEADER_FILE "${TARGET_BIN}/CompiledShaders/${NAMETXT}.h"
            VS_SHADER_OBJECT_FILE_NAME "${TARGET_BIN}/Shaders/${NAMETXT}.cso"
            VS_SHADER_VARIABLE_NAME "g_p${NAMETXT}")
    endforeach()

    set_source_files_properties( ${DX12_COMPUTE_SHADERS} PROPERTIES VS_SHADER_TYPE Compute VS_SHADER_MODEL 5.0)
    set_source_files_properties( ${DX12_PIXEL_SHADERS} PROPERTIES VS_SHADER_TYPE Pixel VS_SHADER_MODEL 5.0)
    set_source_files_properties( ${DX12_VERTEX_SHADERS} PROPERTIES VS_SHADER_TYPE Vertex VS_SHADER_MODEL 5.0)

    LIST(APPEND M3RDPARTY_SOURCE
        ${JORVIK_ROOT}/m3rdparty/imgui/examples/imgui_impl_dx12.cpp
        ${JORVIK_ROOT}/m3rdparty/imgui/examples/imgui_impl_dx12.h
        ${DX12_MINIENGINE_SOURCES}
        ${DX12_COMPUTE_SHADERS} 
        ${DX12_PIXEL_SHADERS}
        ${DX12_VERTEX_SHADERS})

    LIST(APPEND M3RDPARTY_INCLUDE
        m3rdparty/imgui/examples)

ENDIF() 

LIST(APPEND M3RDPARTY_SOURCE
        ${JORVIK_ROOT}/m3rdparty/imgui/examples/imgui_impl_vulkan.cpp
        ${JORVIK_ROOT}/m3rdparty/imgui/examples/imgui_impl_vulkan.h
        ${JORVIK_ROOT}/m3rdparty/imgui/examples/imgui_impl_sdl.cpp
        ${JORVIK_ROOT}/m3rdparty/imgui/examples/imgui_impl_sdl.h
        )

FIND_PACKAGE(Vulkan REQUIRED)

LIST(APPEND M3RDPARTY_INCLUDE
    ${Vulkan_INCLUDE_DIRS}
    )

SET (IMGUI_INCLUDE 
    m3rdparty/imgui/imgui.h
    m3rdparty/imgui/imgui_dock.h
    m3rdparty/imgui/imgui_orient.h)

SET (RTMIDI_SOURCE
	m3rdparty/rtmidi/RtMidi.cpp
	m3rdparty/rtmidi/RtMidi.h)

LIST(APPEND M3RDPARTY_SOURCE 
    ${GAPBUFFER_SOURCE}
    ${FILEWATCHER_SOURCE}
    ${IMGUI_SOURCE}
    ${GL_SOURCE}
    ${MPC_SOURCE}
    ${ZIP_SOURCE}
    ${GLM_SOURCE} ${GLM_INCLUDE}
	${RTMIDI_SOURCE})

IF (TARGET_PC)
LIST(APPEND PLATFORM_LINKLIBS 
    d3d12
    dxgi
    opengl32.lib
    winmm.lib   # SDL - sound, etc.
    version.lib # SDL - windows keyboard
    imm32.lib   # SDL - windows keyboard
    )
ENDIF()

LIST(APPEND PLATFORM_LINKLIBS 
    ${Vulkan_LIBRARIES}
    )

LIST(APPEND M3RDPARTY_INCLUDE
    m3rdparty
    ${CMAKE_BINARY_DIR}
    m3rdparty/imgui
    m3rdparty/glm
    m3rdparty/flatbuffers/include
    m3rdparty/tclap/include
    m3rdparty/gli
    m3rdparty/sdl/include
    m3rdparty/sdl
    m3rdparty/googletest
    )

SET (M3RDPARTY_DIR ${CMAKE_CURRENT_LIST_DIR})

IF (TARGET_PC)
    # Avoid Win compile errors in the zip library
    SET_SOURCE_FILES_PROPERTIES(${ZIP_SOURCE} PROPERTIES COMPILE_FLAGS "/wd4267 /wd4244 /wd4996")
ENDIF()

IF (TARGET_LINUX)
    SET_SOURCE_FILES_PROPERTIES(${ZIP_SOURCE} PROPERTIES COMPILE_FLAGS "-Wno-tautological-compare")
ENDIF()

IF (TARGET_UWP)
    SET_SOURCE_FILES_PROPERTIES(${WINRT} PROPERTIES COMPILE_FLAGS -ZW)
    set_target_properties(${PROJECT_NAME} PROPERTIES STATIC_LIBRARY_FLAGS "/IGNORE:4264")
endif()

# Want ARC on IOS
if (TARGET_IOS)
    target_compile_options(${PROJECT_NAME} PUBLIC "-fobjc-arc")
endif()

INCLUDE(ExternalProject)
ExternalProject_Add(
    sdl2
    PREFIX "m3rdparty"
    CMAKE_ARGS "-DSDL_SHARED=OFF"
    SOURCE_DIR "${M3RDPARTY_DIR}/sdl"
    TEST_COMMAND ""
    INSTALL_COMMAND "" 
    INSTALL_DIR ""
    )
LINK_DIRECTORIES(${CMAKE_BINARY_DIR}/m3rdparty/src/sdl2-build)

IF (PROJECT_FREETYPE)
    ExternalProject_Add(
        freetype
        PREFIX "m3rdparty"
        CMAKE_ARGS "-DFT2_BUILD_LIBRARY=ON"
        SOURCE_DIR "${M3RDPARTY_DIR}/freetype"
        TEST_COMMAND ""
        INSTALL_COMMAND "" 
        INSTALL_DIR ""
        )
    LINK_DIRECTORIES(${CMAKE_BINARY_DIR}/m3rdparty/src/freetype-build)
ENDIF()


IF (PROJECT_SHADERTOOLS)
    include (ExternalProject)
    ExternalProject_Add(
        glslang
        PREFIX "m3rdparty"
        CMAKE_ARGS -DENABLE_AMD_EXTENSIONS=ON -DENABLE_GLSLANG_BINARIES=ON -DENABLE_NV_EXTENSIONS=ON -DENABLE_HLSL=ON
        SOURCE_DIR "${M3RDPARTY_DIR}/glslang"
        TEST_COMMAND ""
        INSTALL_COMMAND "" 
        INSTALL_DIR ""
        )

    ExternalProject_Add(
        spirv-cross
        PREFIX "m3rdparty"
        CMAKE_ARGS "" 
        SOURCE_DIR "${M3RDPARTY_DIR}/spirv-cross"
        TEST_COMMAND ""
        INSTALL_COMMAND "" 
        INSTALL_DIR ""
        )
ENDIF()

IF (TARGET_PC)
	# For midi
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -D__WINDOWS_MM__=1")
    LIST(APPEND PLATFORM_LINKLIBS 
        SDL2
        SDL2main
        )
ENDIF()
IF (TARGET_MAC)
    LIST(APPEND PLATFORM_LINKLIBS 
        SDL2-2.0
        SDL2main
        )
ENDIF()
IF (TARGET_LINUX)
    LIST(APPEND PLATFORM_LINKLIBS 
        SDL2-2.0
        SDL2main
        pthread
        stdc++fs
        )
ENDIF()
SOURCE_GROUP ("m3rdparty\\googletest" REGULAR_EXPRESSION "(googlete)+.*") 
SOURCE_GROUP ("m3rdparty\\glm" REGULAR_EXPRESSION "(glm)+.*") 
SOURCE_GROUP ("m3rdparty\\filewatcher" REGULAR_EXPRESSION "(filewatcher)+.*") 
SOURCE_GROUP ("m3rdparty\\rtmidi" REGULAR_EXPRESSION "(rtmidi)+.*") 
SOURCE_GROUP ("m3rdparty\\imgui" REGULAR_EXPRESSION "(m3rdparty)+.*(imgui)+.*")
SOURCE_GROUP ("m3rdparty\\DX12\\Shaders" REGULAR_EXPRESSION "(m3rdparty)+.*(miniengine)+.*(hl)+.*")
SOURCE_GROUP ("m3rdparty\\DX12" REGULAR_EXPRESSION "(m3rdparty)+.*(miniengine)+.*")
SOURCE_GROUP ("m3rdparty\\GL" REGULAR_EXPRESSION "(GL)+.*")
SOURCE_GROUP ("m3rdparty\\GL" REGULAR_EXPRESSION "(GL)+.*")
SOURCE_GROUP ("m3rdparty\\zip" REGULAR_EXPRESSION "(zip)+.*")
SOURCE_GROUP ("m3rdparty\\mpc" REGULAR_EXPRESSION "(mpc)+.*")

CONFIGURE_FILE(${CMAKE_CURRENT_LIST_DIR}/cmake/config_shared.h.cmake ${CMAKE_BINARY_DIR}/config_shared.h)
