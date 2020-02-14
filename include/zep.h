#pragma once

#ifdef ZEP_SINGLE_HEADER_BUILD
#include "../src/buffer.cpp"
#include "../src/commands.cpp"
#include "../src/editor.cpp"
#include "../src/keymap.cpp"
#include "../src/mode.cpp"
#include "../src/mode_standard.cpp"
#include "../src/mode_vim.cpp"
#include "../src/mode_repl.cpp"
#include "../src/mode_search.cpp"
#include "../src/scroller.cpp"
#include "../src/splits.cpp"
#include "../src/syntax.cpp"
#include "../src/syntax_providers.cpp"
#include "../src/syntax_rainbow_brackets.cpp"
#include "../src/tab_window.cpp"
#include "../src/theme.cpp"
#include "../src/display.cpp"
#include "../src/window.cpp"
#include "../src/filesystem.cpp"
#include "../src/line_widgets.cpp"
#include "../src/mcommon/file/path.cpp"
#include "../src/mcommon/string/stringutils.cpp"
#include "../src/mcommon/animation/timer.cpp"
#include "zep/imgui/display_imgui.h"
#include "zep/imgui/editor_imgui.h"
#else
#include "zep/editor.h"
#include "zep/syntax.h"
#include "zep/buffer.h"
#include "zep/tab_window.h"
#include "zep/mode_vim.h"
#include "zep/mode_standard.h"
#include "zep/window.h"
#include "zep/mode.h"
#include "zep/mode_vim.h"
#include "zep/mode_standard.h"
#ifdef ZEP_QT
#include "zep/qt/display_qt.h"
#include "zep/qt/editor_qt.h"
#else
#include "zep/imgui/display_imgui.h"
#include "zep/imgui/editor_imgui.h"
#include "zep/imgui/console_imgui.h"
#endif
#endif

