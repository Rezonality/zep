#pragma once

#ifdef ZEP_SINGLE_HEADER_BUILD
#include "buffer.cpp"
#include "commands.cpp"
#include "editor.cpp"
#include "imgui/editor_imgui.h"
#include "imgui/display_imgui.h"
#include "mode.cpp"
#include "mode_standard.cpp"
#include "mode_vim.cpp"
#include "scroller.cpp"
#include "splits.cpp"
#include "syntax.cpp"
#include "syntax_providers.cpp"
#include "syntax_rainbow_brackets.cpp"
#include "tab_window.cpp"
#include "theme.cpp"
#include "window.cpp"
#else
#include "editor.h"
#include "syntax.h"
#include "buffer.h"
#endif

