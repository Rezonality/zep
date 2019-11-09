# Build the zep demo
IF (BUILD_DEMOS AND BUILD_IMGUI)

set(DEMO_NAME ZepDemo)

find_package(SDL2 REQUIRED)
find_package(MUtils REQUIRED)

IF(WIN32)
# Let windows know that the app is DPI Aware
set(MANIFEST_SOURCE ${ZEP_ROOT}/dpiawarescaling.manifest)
ENDIF()
set(DEMO_SOURCE_IMGUI
    ${ZEP_ROOT}/demo_imgui/main.cpp
    ${ZEP_ROOT}/demo_imgui/CMakeLists.txt)

add_executable (${DEMO_NAME} WIN32 # Win32 ignored on windows 
    ${DEMO_SOURCE_IMGUI}
    ${MANIFEST_SOURCE})

target_include_directories(${DEMO_NAME}
    PRIVATE
        include
        ${SDL2_ROOT_DIR}
        ${CMAKE_BINARY_DIR}
        ${M3RDPARTY_DIR})

copy_existing_files(${DEMO_NAME} ${MUTILS_INCLUDE_DIR}/imgui/misc/fonts/Cousine-Regular.ttf ${CMAKE_CURRENT_BINARY_DIR}/$(Configuration) )
copy_existing_files(${DEMO_NAME} ${MUTILS_INCLUDE_DIR}/chibi/init-7.scm ${CMAKE_CURRENT_BINARY_DIR}/$(Configuration) )

target_link_libraries (${DEMO_NAME} 
    PRIVATE
        Zep
        MUtils::MUtils
        SDL2::SDL2-static
        SDL2::SDL2main
        ${MUTILS_LIB_DIR}/freetype2/lib/freetype$<$<CONFIG:DEBUG>:d>.lib
        ${PLATFORM_LINKLIBS})

# Install the font
install(FILES ${MUTILS_INCLUDE_DIR}/imgui/misc/fonts/Cousine-Regular.ttf
    DESTINATION ${CMAKE_INSTALL_BINDIR}/imgui
)

install(TARGETS ${DEMO_NAME}
    EXPORT zep-targets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}/imgui
    INCLUDES DESTINATION ${LIBLEGACY_INCLUDE_DIRS}
)

source_group (Zep FILES ${DEMO_SOURCE_IMGUI})

ENDIF() # IMGUI
