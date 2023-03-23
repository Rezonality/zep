# Build the zep demo
IF (BUILD_DEMOS AND BUILD_IMGUI)

set(DEMO_ROOT ${ZEP_ROOT}/demos)
set(DEMO_NAME ZepDemo)

project(ZepDemo
    VERSION 0.1.0.0)

add_project_meta(META_FILES_TO_INCLUDE IS_IMGUI)

find_package(tinyfiledialogs CONFIG REQUIRED)
find_package(SDL2 CONFIG REQUIRED)
find_package(imgui CONFIG REQUIRED)
find_package(gl3w CONFIG REQUIRED)
find_package(Freetype CONFIG REQUIRED)

include(CMakePackageConfigHelpers)

set(CLIP_SOURCE
    ${DEMO_ROOT}/demo_imgui/clip/clip.cpp
    ${DEMO_ROOT}/demo_imgui/clip/image.cpp
    )

if(WIN32)
    LIST(APPEND CLIP_SOURCE
        ${DEMO_ROOT}/demo_imgui/clip/clip_win.cpp)
endif()
if (UNIX)
    if (APPLE)
        LIST(APPEND CLIP_SOURCE
            ${DEMO_ROOT}/demo_imgui/clip/clip_osx.mm)
    else()
        LIST(APPEND CLIP_SOURCE
            ${DEMO_ROOT}/demo_imgui/clip/clip_x11.cpp)
    endif() # APPLE
endif() # UNIX

set(DEMO_SOURCE_IMGUI
    ${DEMO_ROOT}/demo_imgui/janet/janet.c
    ${DEMO_ROOT}/demo_imgui/janet_utils.cpp
    ${DEMO_ROOT}/demo_imgui/main.cpp
    ${DEMO_ROOT}/demo_imgui/dpi/dpi.cpp
    ${DEMO_ROOT}/demo_imgui/dpi/dpi.h
    ${DEMO_ROOT}/demo_imgui/CMakeLists.txt
    ${ZEP_ROOT}/include/zep/imgui/display_imgui.h
    ${ZEP_ROOT}/include/zep/imgui/editor_imgui.h
    ${META_FILES_TO_INCLUDE}
    ${CLIP_SOURCE}
)

add_executable (${DEMO_NAME}
    ${OS_BUNDLE}
    ${DEMO_SOURCE_IMGUI})

target_compile_definitions(${PROJECT_NAME} PUBLIC -DZEP_USE_SDL)

target_include_directories(${DEMO_NAME}
    PRIVATE
    ${ZEP_ROOT}/include
    ${ZEP_ROOT}/demos
    ${ZEP_ROOT}/demos/demo_imgui/dpi
    ${ZEP_ROOT}/demos/demo_imgui/janet
    ${FREETYPE_INCLUDE_DIRS}
    ${SDL2_ROOT_DIR}
    ${CMAKE_BINARY_DIR})

if (NOT WIN32)
    target_include_directories(${DEMO_NAME}
        PRIVATE
        ${FREETYPE_INCLUDE_DIRS})
endif()

target_link_libraries (${DEMO_NAME}
    PRIVATE
    Zep
    $<TARGET_NAME_IF_EXISTS:SDL2::SDL2main>
    $<IF:$<TARGET_EXISTS:SDL2::SDL2>,SDL2::SDL2,SDL2::SDL2-static>
    imgui::imgui
    unofficial::gl3w::gl3w
    tinyfiledialogs::tinyfiledialogs
    ${PLATFORM_LINKLIBS}
    )

if (WIN32)
    copy_existing_files(${PROJECT_NAME} "${RESOURCE_DEPLOY_FILES}" ${CMAKE_CURRENT_BINARY_DIR}/$(Configuration) )
else()
    target_link_libraries (${DEMO_NAME}
        PRIVATE
        ${FREETYPE_LIBRARIES}
        )
endif()

# Install the font
install(FILES ${DEMO_ROOT}/demo_imgui/res/Cousine-Regular.ttf
    COMPONENT binaries
    DESTINATION ${CMAKE_INSTALL_BINDIR}/imgui
    )

if (APPLE)
    install(TARGETS ${DEMO_NAME}
        EXPORT zep-targets
        COMPONENT binaries
        BUNDLE DESTINATION . COMPONENT Runtime
        RUNTIME DESTINATION bin COMPONENT Runtime
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        INCLUDES DESTINATION ${LIBLEGACY_INCLUDE_DIRS})
else()
    install(TARGETS ${DEMO_NAME}
        EXPORT zep-targets
        COMPONENT binaries
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}/imgui
        INCLUDES DESTINATION ${LIBLEGACY_INCLUDE_DIRS}
        )
endif()

source_group (Zep FILES ${DEMO_SOURCE_IMGUI})

ENDIF() # IMGUI

