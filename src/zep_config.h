#pragma once

#if defined(__linux__) || defined(__linux) || defined(linux__) || defined(__unix__) 
// Linux
#elif defined(__APPLE__)
// Apple - file watcher disabled for onw
#undef ZEP_FEATURE_FILE_WATCHER
#elif defined(_WIN32)
// Windows
#endif


