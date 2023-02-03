# Build the zep demo
IF (BUILD_DEMOS AND BUILD_IMGUI)

set(DEMO_ROOT ${ZEP_ROOT}/demos)
set(DEMO_NAME ZepDemo)

project(ZepDemo
    VERSION 0.1.0.0)

add_project_meta(META_FILES_TO_INCLUDE IS_IMGUI)

find_package(gsl-lite CONFIG REQUIRED)
find_package(unofficial-concurrentqueue CONFIG REQUIRED)
find_package(tinyfiledialogs CONFIG REQUIRED)
find_package(fmt CONFIG REQUIRED)
find_package(SDL2 CONFIG REQUIRED)
find_package(clipp CONFIG REQUIRED)
find_package(imgui CONFIG REQUIRED)
find_package(gl3w CONFIG REQUIRED)
find_package(freetype CONFIG REQUIRED)
find_package(MUtils CONFIG REQUIRED)


include(CMakePackageConfigHelpers)

set(DEMO_SOURCE_IMGUI
    ${DEMO_ROOT}/demo_imgui/main.cpp
    ${DEMO_ROOT}/demo_imgui/CMakeLists.txt
    ${ZEP_ROOT}/include/zep/imgui/display_imgui.h
    ${ZEP_ROOT}/include/zep/imgui/editor_imgui.h
    ${META_FILES_TO_INCLUDE}
    )

add_executable (${DEMO_NAME}
    ${OS_BUNDLE}
    ${DEMO_SOURCE_IMGUI})

target_compile_definitions(${PROJECT_NAME} PUBLIC -DZEP_USE_SDL)

target_include_directories(${DEMO_NAME}
    PRIVATE
    ${ZEP_ROOT}/include
    ${ZEP_ROOT}/demos
    ${FREETYPE_INCLUDE_DIRS}
    ${SDL2_ROOT_DIR}
    ${CMAKE_BINARY_DIR}
    ${M3RDPARTY_DIR})

if (NOT WIN32)
    target_include_directories(${DEMO_NAME}
        PRIVATE
        ${FREETYPE_INCLUDE_DIRS})
endif()

target_link_libraries (${DEMO_NAME}
    PRIVATE
    Zep
    ZepExtensions
    $<TARGET_NAME_IF_EXISTS:SDL2::SDL2main>
    $<IF:$<TARGET_EXISTS:SDL2::SDL2>,SDL2::SDL2,SDL2::SDL2-static>
    MUtils::MUtils
    imgui::imgui
    clipp::clipp
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

