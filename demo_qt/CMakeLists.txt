IF (BUILD_DEMOS AND BUILD_QT)

# Qt Specific
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_PREFIX_PATH $ENV{QT_INSTALL_LOCATION})

find_package(Qt5 COMPONENTS Core Widgets Gui REQUIRED)
set_property(GLOBAL PROPERTY AUTOMOC_FOLDER Automoc)
set_property(GLOBAL PROPERTY AUTOGEN_TARGETS_FOLDER Automoc)

set(DEMO_NAME ZepDemo-qt)

set(RESOURCE_FOLDER 
    ${ZEP_ROOT}/demo_qt/res)

set(RESOURCE_FILES
    ${ZEP_ROOT}/demo_qt/app.qrc
    ${ZEP_ROOT}/demo_qt/app.rc)

set(DEMO_SOURCE_QT
    ${ZEP_ROOT}/demo_qt/CMakeLists.txt
    ${ZEP_ROOT}/demo_qt/mainwindow.cpp
    ${ZEP_ROOT}/demo_qt/mainwindow.h
    ${ZEP_ROOT}/demo_qt/resource.h
    ${ZEP_ROOT}/demo_qt/app.exe.manifest
    ${ZEP_ROOT}/demo_qt/main-qt.cpp
    ${ZEP_ROOT}/demo_qt/app.rc
    ${ZEP_ROOT}/include/zep/qt/zepdisplay_qt.h
    ${ZEP_ROOT}/include/zep/qt/zepwidget_qt.h
    ${ZEP_ROOT}/include/zep/qt/common_qt.h
)

# Create the Qt version of the app
add_executable (${DEMO_NAME} WIN32 ${DEMO_SOURCE_QT} ${RESOURCE_FILES}) # Win32 ignored on non-windows
set_property(TARGET ${DEMO_NAME} PROPERTY POSITION_INDEPENDENT_CODE ON) # Qt requires this?
target_include_directories(${DEMO_NAME}
    PRIVATE
        ${ZEP_ROOT}/demo_qt
        ${ZEP_ROOT}/src/qt
        ${ZEP_ROOT}/extensions
        ${Qt5Widgets_INCLUDE_DIRS}
        ${Qt5Gui_INCLUDE_DIRS}
        ${Qt5Core_INCLUDE_DIRS}
        ${CMAKE_BINARY_DIR}
        ${M3RDPARTY_DIR}
        include)

# For the scheme integration demo
copy_existing_files(${DEMO_NAME} ${MUTILS_INCLUDE_DIR}/chibi/init-7.scm ${CMAKE_CURRENT_BINARY_DIR}/$(Configuration) )

target_link_libraries(${DEMO_NAME} PRIVATE Qt5::Core Qt5::Gui Qt5::Widgets MUtils::MUtils Zep ${PLATFORM_LINKLIBS})
target_compile_options(${DEMO_NAME} PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/W3>) # Workaround Qt + MSVC 19 compile issue in release build.
add_dependencies(${DEMO_NAME} Zep)

if(WIN32)
message(STATUS "Copying required Qt libraries and binaries to output directory....")
# Run winddeployqt if it can be found, to ensure installed dependencies
find_program(WINDEPLOYQT_EXECUTABLE NAMES windeployqt HINTS ${QTDIR} ENV QTDIR PATH_SUFFIXES bin)
add_custom_command(TARGET ${DEMO_NAME} POST_BUILD
    COMMAND ${WINDEPLOYQT_EXECUTABLE} $<TARGET_FILE:${DEMO_NAME}>)
endif() # Win32

install(TARGETS ${DEMO_NAME} 
    EXPORT zep-targets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}/qt
    INCLUDES DESTINATION ${LIBLEGACY_INCLUDE_DIRS}
)

install(DIRECTORY ${ZEP_ROOT}/include/zep
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
    )
endif()

source_group (Zep FILES ${DEMO_SOURCE_QT})
source_group(qt\\AutoMoc FILES ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}_automoc.cpp ) 
source_group(qt\\AutoMoc REGULAR_EXPRESSION "(mocs_*|qrc_.*|QtAwesome.*)" ) 

endif() # Build QT
