IF (BUILD_DEMOS AND BUILD_QT)

set(DEMO_ROOT ${ZEP_ROOT}/demos)

# Qt Specific
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_PREFIX_PATH $ENV{QT_INSTALL_LOCATION})

# Note: The SDL2 package is required by the MUtils dependency; it isn't really used for the Qt demo
find_package(SDL2 REQUIRED)
find_package(MUtils REQUIRED)
find_package(Qt5 COMPONENTS Core Widgets Gui REQUIRED)
set_property(GLOBAL PROPERTY AUTOMOC_FOLDER Automoc)
set_property(GLOBAL PROPERTY AUTOGEN_TARGETS_FOLDER Automoc)

# Find Mac and Win deploy
find_program(WINDEPLOYQT_EXECUTABLE NAMES windeployqt HINTS ${QTDIR} ENV QTDIR PATH_SUFFIXES bin)
find_program(MACDEPLOYQT_EXECUTABLE NAMES macdeployqt HINTS ${QTDIR} ENV QTDIR PATH_SUFFIXES bin)

set(DEMO_NAME ZepDemo-qt)
project(ZepDemo-qt 
    VERSION 0.1.0.0)

set(IS_QT YES)
add_project_meta(META_FILES_TO_INCLUDE IS_QT)

set(RESOURCE_FOLDER 
    ${DEMO_ROOT}/demo_qt/res)

set(RESOURCE_FILES
    ${DEMO_ROOT}/demo_qt/res/app.qrc)

set(DEMO_SOURCE_QT
    ${DEMO_ROOT}/demo_qt/CMakeLists.txt
    ${DEMO_ROOT}/demo_qt/mainwindow.cpp
    ${DEMO_ROOT}/demo_qt/mainwindow.h
    ${DEMO_ROOT}/demo_qt/resource.h
    ${DEMO_ROOT}/demo_qt/main-qt.cpp
    ${ZEP_ROOT}/include/zep/qt/zepdisplay_qt.h
    ${ZEP_ROOT}/include/zep/qt/zepwidget_qt.h
    ${ZEP_ROOT}/include/zep/qt/common_qt.h
    ${META_FILES_TO_INCLUDE}
)

# Create the Qt version of the app
add_executable (${DEMO_NAME} ${OS_BUNDLE} ${DEMO_SOURCE_QT} ${RESOURCE_FILES}) # Win32 ignored on non-windows
set_property(TARGET ${DEMO_NAME} PROPERTY POSITION_INDEPENDENT_CODE ON) # Qt requires this?

target_include_directories(${DEMO_NAME}
    PRIVATE
        ${DEMO_ROOT}/demo_qt
        ${ZEP_ROOT}/src/qt
        ${ZEP_ROOT}/demos
        ${Qt5Widgets_INCLUDE_DIRS}
        ${Qt5Gui_INCLUDE_DIRS}
        ${Qt5Core_INCLUDE_DIRS}
        ${CMAKE_BINARY_DIR}
        ${M3RDPARTY_DIR}
        ${ZEP_ROOT}/include)

target_link_libraries(${DEMO_NAME} PRIVATE 
    Qt5::Core
    Qt5::Gui
    Qt5::Widgets
    Zep
    ZepExtensions
    MUtils::MUtils 
    ${PLATFORM_LINKLIBS})

target_compile_options(${DEMO_NAME} PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/W3>) # Workaround Qt + MSVC 19 compile issue in release build.
add_dependencies(${DEMO_NAME} Zep ZepExtensions)

if(WIN32)
message(STATUS "Copying required Qt libraries and binaries to output directory....")
copy_existing_files(${PROJECT_NAME} "${RESOURCE_DEPLOY_FILES}" ${CMAKE_CURRENT_BINARY_DIR}/$(Configuration) )
# Run winddeployqt if it can be found, to ensure installed dependencies
add_custom_command(TARGET ${DEMO_NAME} POST_BUILD
    COMMAND ${WINDEPLOYQT_EXECUTABLE} $<TARGET_FILE:${DEMO_NAME}>)
elseif(APPLE)
add_custom_command(TARGET ${DEMO_NAME} POST_BUILD
    COMMAND "${MACDEPLOYQT_EXECUTABLE}"
    "$<TARGET_FILE_DIR:${DEMO_NAME}>/../.."
        -always-overwrite
    COMMENT "Running macdeployqt...")
endif()

if (APPLE)
install(TARGETS ${DEMO_NAME}
    EXPORT zep-targets
    COMPONENT binaries
    BUNDLE DESTINATION . COMPONENT Runtime
    RUNTIME DESTINATION bin COMPONENT Runtime
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    INCLUDES DESTINATION ${LIBLEGACY_INCLUDE_DIRS}
    )

else()
install(TARGETS ${DEMO_NAME} 
    EXPORT zep-targets
    COMPONENT binaries
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}/qt
    INCLUDES DESTINATION ${LIBLEGACY_INCLUDE_DIRS}
)
endif()


install(DIRECTORY ${ZEP_ROOT}/include/zep
    COMPONENT source
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

# NOTE:
# This unfortunate mess is handling the problem of deploying Qt into the install folder.
# It takes the Qt deploy application output into a folder and copies it to the install location
if(Qt5_FOUND AND WIN32 AND TARGET Qt5::qmake AND NOT TARGET Qt5::windeployqt)
    MESSAGE(STATUS "Found Qt for deploy")
    get_target_property(_qt5_qmake_location Qt5::qmake IMPORTED_LOCATION)

    execute_process(
        COMMAND "${_qt5_qmake_location}" -query QT_INSTALL_PREFIX
        RESULT_VARIABLE return_code
        OUTPUT_VARIABLE qt5_install_prefix
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )

    set(imported_location "${qt5_install_prefix}/bin/windeployqt.exe")

    if(EXISTS ${imported_location})
        add_executable(Qt5::windeployqt IMPORTED)

        set_target_properties(Qt5::windeployqt PROPERTIES
            IMPORTED_LOCATION ${imported_location}
        )
    endif()
endif()

if(TARGET Qt5::windeployqt)
    # execute windeployqt in a tmp directory after build
    add_custom_command(TARGET ${DEMO_NAME}
        POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E remove_directory "${CMAKE_CURRENT_BINARY_DIR}/windeployqt"
        COMMAND set PATH=%PATH%$<SEMICOLON>${qt5_install_prefix}/bin
        COMMAND Qt5::windeployqt --dir "${CMAKE_CURRENT_BINARY_DIR}/windeployqt" "$<TARGET_FILE_DIR:${DEMO_NAME}>/$<TARGET_FILE_NAME:${DEMO_NAME}>"
    )

    # copy deployment directory during installation
    install(
        DIRECTORY
        "${CMAKE_CURRENT_BINARY_DIR}/windeployqt/"
        DESTINATION ${CMAKE_INSTALL_BINDIR}/qt
        COMPONENT binaries
    )
endif()

source_group (Zep FILES ${DEMO_SOURCE_QT})
source_group(qt\\AutoMoc FILES ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}_automoc.cpp ) 
source_group(qt\\AutoMoc REGULAR_EXPRESSION "(mocs_*|qrc_.*|QtAwesome.*)" ) 

endif() # Build QT
