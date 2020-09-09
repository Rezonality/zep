IF (BUILD_EXTENSIONS)
project(ZepExtensions
    VERSION 0.1.0.0)

#add_compile_options("$<$<CONFIG:DEBUG>:-DTRACY_ENABLE=1>")
add_compile_options("$<$<CONFIG:RELWITHDEBINFO>:-DTRACY_ENABLE=1>")

find_package(SDL2 REQUIRED)
find_package(MUtils REQUIRED)

SET(EXTENSION_ROOT ${CMAKE_CURRENT_LIST_DIR})

SET(ORCA_SRC 
    ${EXTENSION_ROOT}/orca/mode_orca.cpp
    ${EXTENSION_ROOT}/orca/mode_orca.h
    ${EXTENSION_ROOT}/orca/orca.cpp
    ${EXTENSION_ROOT}/orca/orca.h
    ${EXTENSION_ROOT}/orca/syntax_orca.cpp
    ${EXTENSION_ROOT}/orca/syntax_orca.h
    )

SET(REPL_SRC 
    ${EXTENSION_ROOT}/repl/mode_repl.cpp
    ${EXTENSION_ROOT}/repl/mode_repl.h)

SET(SOURCES ${REPL_SRC} ${ORCA_SRC})
SET(ZEP_EXTENSION_SRC ${SOURCES} PARENT_SCOPE)

# Create the library - No depenencies on anything else in this file
add_library(ZepExtensions STATIC ${SOURCES})
add_library(Zep::ZepExtensions ALIAS ZepExtensions)

target_include_directories(ZepExtensions
    PRIVATE
        ${ZEP_ROOT}/src/mcommon
        ${ZEP_ROOT}/src
        ${SRC_INCLUDE}
        ${CMAKE_BINARY_DIR}
        ${ZEP_ROOT}/include
        ${ZEP_ROOT}/extensions
    PUBLIC
        $<INSTALL_INTERFACE:include/zep/extensions> # This is the target path in the installer
        $<BUILD_INTERFACE:${ZEP_ROOT}/extensions> # This is the target path in the installe
    ) 

target_link_libraries (${PROJECT_NAME} 
    PUBLIC
    Zep
    MUtils::MUtils
    SDL2::SDL2-static
    SDL2::SDL2main
    ${PLATFORM_LINKLIBS})

# Set locations for components
set_target_properties(${PROJECT_NAME} PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

# Install the binary
install(TARGETS ${PROJECT_NAME}
    EXPORT zep-targets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    INCLUDES DESTINATION ${LIBLEGACY_INCLUDE_DIRS}
)

# Install the extension files
install(DIRECTORY ${ZEP_ROOT}/extensions/orca
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/zep/extensions
)
install(DIRECTORY ${ZEP_ROOT}/extensions/repl
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/zep/extensions)

endif()
