/**
 * @file  test-linux.c
 * @brief cfgpath.h test code for the Linux platform.
 *
 * Copyright (C) 2013 Adam Nielsen <malvineous@shikadi.net>
 *
 * This code is placed in the public domain.  You are free to use it for any
 * purpose.  If you add new platform support, please contribute a patch!
 */

#include <string.h>
#include <stdio.h>

#define SHGetFolderPath test_SHGetFolderPath
#define mkdir test_mkdir
#undef __linux__
#ifndef WIN32
#define WIN32
#endif
#include "cfgpath.h"

const char *set_appdata;
const char *set_appdata_local;
int set_retval;

/* Fake implementation of SHGetFolderPath() that fails when and how we want */
int test_SHGetFolderPath(void *hwndOwner, int nFolder, void *hToken,
	int dwFlags, char *pszPath)
{
	/* Hopefully trigger an error if the buffer is too small */
	pszPath[MAX_PATH - 1] = 0;

	switch (nFolder) {
		case CSIDL_APPDATA:
			if (set_appdata) {
				strcpy(pszPath, set_appdata);
				return set_retval;
			}
			break;
		case CSIDL_LOCAL_APPDATA:
			if (set_appdata_local) {
				strcpy(pszPath, set_appdata_local);
				return set_retval;
			}
			break;
	}
	return E_FAIL;
}

int test_mkdir(const char *path)
{
	return 0;
}

#define TOSTRING_X(x) #x
#define TOSTRING(x) TOSTRING_X(x)
#define RUN_TEST(result, msg)	  \
	TEST_FUNC(buffer, sizeof(buffer), "test-win"); \
	CHECK_RESULT(result, msg)

#define CHECK_RESULT(result, msg) \
	if (strcmp(buffer, result) != 0) { \
		printf("FAIL: %s:%d " TOSTRING(TEST_FUNC) "() " msg "  Test failed, " \
			"returning the wrong value.\n" \
			"Expected: %s\nGot: %s\n", __FILE__, __LINE__, result, buffer); \
		return 1; \
	} else { \
		printf("PASS: " TOSTRING(TEST_FUNC) "() " msg "\n"); \
	}

int main(int argc, char *argv[])
{
	char buffer[256];

/*
 * get_user_config_file()
 */

#define TEST_RESULT "C:\\Users\\test-win\\AppData\\Roaming\\test-win.ini"
#define TEST_FUNC get_user_config_file

	set_retval = S_OK;
	set_appdata = "C:\\Users\\test-win\\AppData\\Roaming";

	TEST_FUNC(buffer, 5, "test-win");
	CHECK_RESULT("", "returns empty string when buffer is too small.");

	RUN_TEST(TEST_RESULT, "works with CSIDL_APPDATA.");

	set_retval = S_FALSE;
	RUN_TEST(TEST_RESULT, "works if folder doesn't exist (S_FALSE).");

	set_appdata = NULL;
	RUN_TEST("", "fails with missing CSIDL_APPDATA.");

#undef TEST_FUNC
#undef TEST_RESULT

/*
 * get_user_config_folder()
 */

#define TEST_RESULT "C:\\Users\\test-win\\AppData\\Roaming\\test-win\\"
#define TEST_FUNC get_user_config_folder

	set_retval = S_OK;
	set_appdata = "C:\\Users\\test-win\\AppData\\Roaming";

	TEST_FUNC(buffer, 5, "test-win");
	CHECK_RESULT("", "returns empty string when buffer is too small.");

	RUN_TEST(TEST_RESULT, "works with CSIDL_APPDATA.");

	set_retval = S_FALSE;
	RUN_TEST(TEST_RESULT, "works if folder doesn't exist (S_FALSE).");

	set_appdata = NULL;
	RUN_TEST("", "fails with missing CSIDL_APPDATA.");

#undef TEST_FUNC
#undef TEST_RESULT

/*
 * get_user_data_folder()
 */

#define TEST_RESULT "C:\\Users\\test-win\\AppData\\Roaming\\test-win\\"
#define TEST_FUNC get_user_data_folder

	set_retval = S_OK;
	set_appdata = "C:\\Users\\test-win\\AppData\\Roaming";

	TEST_FUNC(buffer, 5, "test-win");
	CHECK_RESULT("", "returns empty string when buffer is too small.");

	RUN_TEST(TEST_RESULT, "works with CSIDL_APPDATA.");

	set_retval = S_FALSE;
	RUN_TEST(TEST_RESULT, "works if folder doesn't exist (S_FALSE).");

	set_appdata = NULL;
	RUN_TEST("", "fails with missing CSIDL_APPDATA.");

#undef TEST_FUNC
#undef TEST_RESULT

/*
 * get_user_cache_folder()
 */

#define TEST_RESULT "C:\\Users\\test-win\\AppData\\Local\\test-win\\"
#define TEST_FUNC get_user_cache_folder

	set_retval = S_OK;
	set_appdata_local = "C:\\Users\\test-win\\AppData\\Local";

	TEST_FUNC(buffer, 5, "test-win");
	CHECK_RESULT("", "returns empty string when buffer is too small.");

	RUN_TEST(TEST_RESULT, "works with CSIDL_LOCAL_APPDATA.");

	set_retval = S_FALSE;
	RUN_TEST(TEST_RESULT, "works if folder doesn't exist (S_FALSE).");

	set_appdata_local = NULL;
	RUN_TEST("", "fails with missing CSIDL_LOCAL_APPDATA.");

#undef TEST_FUNC
#undef TEST_RESULT

	printf("All tests passed for platform: Windows.\n");
	return 0;
}
