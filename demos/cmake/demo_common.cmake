find_package(SDL2 REQUIRED)
find_package(MUtils REQUIRED)

macro(add_project_meta FILES_TO_INCLUDE IS_QT)
if (NOT RESOURCE_FOLDER)
    MESSAGE(STATUS ${PROJECT_SOURCE_DIR})
    set(RESOURCE_FOLDER ${PROJECT_SOURCE_DIR}/res)
endif()

if (NOT ICON_NAME)
    set(ICON_NAME AppIcon)
endif()

set(RESOURCE_DEPLOY_FILES 
    ${MUTILS_INCLUDE_DIR}/chibi/init-7.scm
    ${ZEP_ROOT}/zep.cfg)

if (APPLE)
    set(ICON_FILE ${RESOURCE_FOLDER}/${ICON_NAME}.icns)
elseif (WIN32)
    set(ICON_FILE ${RESOURCE_FOLDER}/${ICON_NAME}.ico)
endif()

if (NOT IS_QT)
    set(RESOURCE_DEPLOY_FILES ${RESOURCE_DEPLOY_FILES}
        ${MUTILS_INCLUDE_DIR}/imgui/misc/fonts/Cousine-Regular.ttf
        ${MUTILS_INCLUDE_DIR}/imgui/misc/fonts/DroidSans.ttf
        ${MUTILS_INCLUDE_DIR}/imgui/misc/fonts/Roboto-Medium.ttf)
endif()

if (WIN32)
    configure_file("${DEMO_ROOT}/cmake/windows_metafile.rc.in"
      "windows_metafile.rc"
    )
    set(RES_FILES windows_metafile.rc ${PROJECT_SOURCE_DIR}/res/app.manifest)
endif()

if (APPLE)
    set_source_files_properties(${ICON_FILE} PROPERTIES MACOSX_PACKAGE_LOCATION Resources)
    set_source_files_properties(${RESOURCE_DEPLOY_FILES} PROPERTIES MACOSX_PACKAGE_LOCATION Resources)

    # Identify MacOS bundle
    set(MACOSX_BUNDLE_BUNDLE_NAME ${PROJECT_NAME})
    set(MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION})
    set(MACOSX_BUNDLE_LONG_VERSION_STRING ${PROJECT_VERSION})
    set(MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}")
    set(MACOSX_BUNDLE_COPYRIGHT ${COPYRIGHT})
    set(MACOSX_BUNDLE_GUI_IDENTIFIER ${IDENTIFIER})
    set(MACOSX_BUNDLE_ICON_FILE ${ICON_NAME})
endif()

if (APPLE)
    set(${FILES_TO_INCLUDE} ${ICON_FILE} ${RESOURCE_DEPLOY_FILES})
elseif (WIN32)
    set(${FILES_TO_INCLUDE} ${RES_FILES})
endif()
endmacro()

macro(init_os_bundle)
if (APPLE)
    set(OS_BUNDLE MACOSX_BUNDLE)
elseif (WIN32)
    set(OS_BUNDLE WIN32)
endif()
endmacro()

macro(fix_win_compiler)
if (MSVC)
    set_target_properties(${PROJECT_NAME} PROPERTIES
        WIN32_EXECUTABLE YES
        LINK_FLAGS "/ENTRY:mainCRTStartup"
    )
endif()
endmacro()

macro(init_qt)
# Let's do the CMake job for us
set(CMAKE_AUTOMOC ON) # For meta object compiler
set(CMAKE_AUTORCC ON) # Resource files
set(CMAKE_AUTOUIC ON) # UI files
endmacro()

init_os_bundle()


