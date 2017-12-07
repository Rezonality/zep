#pragma once

// Don't let windows include the silly min/max macro
#define NOMINMAX

#include <cstdlib>

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

// Standard library, used all the time.
#include <cstdint>  // uint64_t, etc.
#include <cctype>
#include <string>
#include <chrono>     // Timing
#include <algorithm>  // Various useful things
#include <memory>     // shared_ptr
#include <sstream>

// Containers, used all the time.
#include <vector>
#include <map>

// IO
#include <iostream>
#include <fstream>

