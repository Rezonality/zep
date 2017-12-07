
SET(ZEP_SOURCE
src/utils/timer.cpp
src/utils/timer.h
src/utils/stringutils.cpp
src/utils/stringutils.h
src/utils/threadutils.h
src/editor.cpp
src/editor.h
src/buffer.cpp
src/buffer.h
src/commands.cpp
src/commands.h
src/display.cpp
src/display.h
src/window.cpp
src/window.h
src/syntax.cpp
src/syntax.h
src/syntax_glsl.cpp
src/syntax_glsl.h
src/mode.cpp
src/mode.h
src/mode_standard.cpp
src/mode_standard.h
src/mode_vim.cpp
src/mode_vim.h
src/list.cmake
)

LIST(APPEND SRC_INCLUDE src)

IF (BUILD_QT)
SET(ZEP_SOURCE_QT
    src/qt/display_qt.cpp
    src/qt/display_qt.h
    src/qt/window_qt.cpp
    src/qt/window_qt.h
    )
SET(ZEP_INCLUDE_QT src/qt)
ENDIF()

IF (BUILD_IMGUI)
SET(ZEP_SOURCE_IMGUI
    src/imgui/display_imgui.cpp
    src/imgui/display_imgui.h
    src/imgui/editor_imgui.cpp
    src/imgui/editor_imgui.h)
SET(ZEP_INCLUDE_IMGUI src/qt)
ENDIF()

