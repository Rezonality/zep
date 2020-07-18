# Build the zep demo
IF (BUILD_DEMOS AND BUILD_IMGUI)

add_definitions(-DZEP_USE_SDL)

set(DEMO_ROOT ${ZEP_ROOT}/demos)
set(DEMO_NAME ZepDemo)
project(ZepDemo
    VERSION 0.1.0.0)

if (WIN32)
add_compile_options("$<$<CONFIG:DEBUG>:-DTRACY_ENABLE=1>")
add_compile_options("$<$<CONFIG:RELWITHDEBINFO>:-DTRACY_ENABLE=1>")
endif()

add_project_meta(META_FILES_TO_INCLUDE IS_IMGUI)

find_package(SDL2 REQUIRED)
find_package(MUtils REQUIRED)

if (NOT WIN32)
    find_package(Freetype REQUIRED)
endif()

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
    MUtils::MUtils
    SDL2::SDL2-static
    SDL2::SDL2main
    ${PLATFORM_LINKLIBS}
    )

if (WIN32)
    # No longer needed?
    #target_link_libraries (${DEMO_NAME} 
    #    PRIVATE
    # ${MUTILS_LIB_DIR}/freetype2/lib/freetype$<$<CONFIG:DEBUG>:d>.lib
    #    )
    copy_existing_files(${PROJECT_NAME} "${RESOURCE_DEPLOY_FILES}" ${CMAKE_CURRENT_BINARY_DIR}/$(Configuration) )
else()
    target_link_libraries (${DEMO_NAME} 
        PRIVATE
        ${FREETYPE_LIBRARIES}
        )
endif()

# Install the font
install(FILES ${MUTILS_INCLUDE_DIR}/imgui/misc/fonts/Cousine-Regular.ttf
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

