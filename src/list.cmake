
SET(ZEP_ROOT ${CMAKE_CURRENT_LIST_DIR}/../)
SET(ZEP_SOURCE
${ZEP_ROOT}/src/mcommon/animation/timer.cpp
${ZEP_ROOT}/src/mcommon/string/stringutils.cpp
${ZEP_ROOT}/src/mcommon/file/archive.cpp
${ZEP_ROOT}/src/mcommon/file/path.cpp
${ZEP_ROOT}/src/filesystem.cpp
${ZEP_ROOT}/src/editor.cpp
${ZEP_ROOT}/src/splits.cpp
${ZEP_ROOT}/src/buffer.cpp
${ZEP_ROOT}/src/commands.cpp
${ZEP_ROOT}/src/display.cpp
${ZEP_ROOT}/src/tab_window.cpp
${ZEP_ROOT}/src/window.cpp
${ZEP_ROOT}/src/scroller.cpp
${ZEP_ROOT}/src/syntax.cpp
${ZEP_ROOT}/src/syntax_providers.cpp
${ZEP_ROOT}/src/syntax_rainbow_brackets.cpp
${ZEP_ROOT}/src/mode.cpp
${ZEP_ROOT}/src/mode_standard.cpp
${ZEP_ROOT}/src/mode_vim.cpp
${ZEP_ROOT}/src/mode_search.cpp
${ZEP_ROOT}/src/theme.cpp
${ZEP_ROOT}/src/list.cmake

${ZEP_ROOT}/include/zep/filesystem.h
${ZEP_ROOT}/include/zep/editor.h
${ZEP_ROOT}/include/zep/splits.h
${ZEP_ROOT}/include/zep/buffer.h
${ZEP_ROOT}/include/zep/commands.h
${ZEP_ROOT}/include/zep/window.h
${ZEP_ROOT}/include/zep/scroller.h
${ZEP_ROOT}/include/zep/syntax.h
${ZEP_ROOT}/include/zep/theme.h
${ZEP_ROOT}/include/zep/mode_search.h
${ZEP_ROOT}/include/zep/mode_standard.h
${ZEP_ROOT}/include/zep/mode_vim.h
${ZEP_ROOT}/include/zep/mode.h
${ZEP_ROOT}/include/zep/syntax_rainbow_brackets.h
${ZEP_ROOT}/include/zep/syntax_providers.h
${ZEP_ROOT}/include/zep/tab_window.h
${ZEP_ROOT}/include/zep/display.h

${ZEP_ROOT}/include/zep/mcommon/animation/timer.h
${ZEP_ROOT}/include/zep/mcommon/string/stringutils.h
${ZEP_ROOT}/include/zep/mcommon/threadutils.h
${ZEP_ROOT}/include/zep/mcommon/file/archive.h
${ZEP_ROOT}/include/zep/mcommon/file/path.h
${ZEP_ROOT}/include/zep/mcommon/logger.h
)

LIST(APPEND SRC_INCLUDE ${ZEP_ROOT}/src ${ZEP_ROOT}/src/mcommon)

IF (BUILD_QT)
SET(ZEP_SOURCE_QT
    ${ZEP_ROOT}/src/qt/zepdisplay_qt.cpp
    ${ZEP_ROOT}/src/qt/zepwidget_qt.cpp
    
    ${ZEP_ROOT}/include/zep/qt/zepwidget_qt.h
    ${ZEP_ROOT}/include/zep/qt/zepdisplay_qt.h
    )
SET(ZEP_INCLUDE_QT ${ZEP_ROOT}/src/qt)
ENDIF()

IF (BUILD_IMGUI)
SET(ZEP_SOURCE_IMGUI
    ${ZEP_ROOT}/src/imgui/display_imgui.cpp
    ${ZEP_ROOT}/src/imgui/editor_imgui.cpp
    ${ZEP_ROOT}/include/zep/imgui/editor_imgui.h
    ${ZEP_ROOT}/include/zep/imgui/display_imgui.h)

SET(ZEP_INCLUDE_IMGUI ${ZEP_ROOT}/src/qt)
ENDIF()

