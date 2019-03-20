#pragma once

// Don't let windows include the silly min/max macro
#define NOMINMAX

#define mynew new

#ifdef _DEBUG
#if defined(_MSC_VER)
// Memory leak/corruption detection for Windows
#define _CRTDBG_MAP_ALLOC
#undef mynew
#define mynew new ( _NORMAL_BLOCK , __FILE__ , __LINE__ )
#include <crtdbg.h>
#endif // MSVC
#endif // _DEBUG

// Include GLM Math library
#define GLM_ENABLE_EXPERIMENTAL
#define GLM_LANG_STL11_FORCED
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
//#define GLM_FORCE_LEFT_HANDED

#include "config_shared.h"
