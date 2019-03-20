/**
 * @file  shlobj.h
 * @brief Dummy file to allow running the Win32 tests on non-Windows platforms.
 *
 * Copyright (C) 2013 Adam Nielsen <malvineous@shikadi.net>
 *
 * This code is placed in the public domain.  You are free to use it for any
 * purpose.  If you add new platform support, please contribute a patch!
 */

#ifdef CSIDL_APPDATA
#error This file should not be used under Windows, use the system version instead!
#endif

#define CSIDL_APPDATA       26 /* roaming appdata */
#define CSIDL_LOCAL_APPDATA 28

#define S_OK 0
#define S_FALSE 1
#define E_FAIL -1

#define SUCCEEDED(hr) ((hr) >= 0)

#define MAX_PATH 256
