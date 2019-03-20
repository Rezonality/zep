#include <stdio.h>

#define UNICODE
#include <tinydir.h>
#include "cbehave.h"


#define TEST_PATH L"windows_unicode_test"
#define TEST_PATH_A "windows_unicode_test"

void make_temp_file_utf16(const wchar_t *prefix, wchar_t *out)
{
	if (GetTempFileNameW(TEST_PATH, prefix, 0, out) != 0)
	{
		// Create file
		fclose(_wfopen(out, L"w"));
		// Strip the ".\\" prefix
		if (wcsncmp(out, TEST_PATH L"\\", wcslen(TEST_PATH) + 1) == 0)
		{
			memmove(out, out + wcslen(TEST_PATH) + 1, wcslen(out));
		}
	}
}

FEATURE(windows_unicode, "Windows Unicode")
	SCENARIO("Open directory with unicode file names")
		GIVEN("a directory with unicode file names")
			CreateDirectoryW(TEST_PATH, NULL);
			wchar_t name[4096];
			make_temp_file_utf16(L"t📁", name);
		WHEN("we open the directory")
			tinydir_dir dir;
			int r = tinydir_open(&dir, TEST_PATH);
		THEN("the result should be successful")
			SHOULD_INT_EQUAL(r, 0);
		AND("iterating it, we should find the file")
			bool found = false;
			while (dir.has_next)
			{
				tinydir_file file;
				tinydir_readfile(&dir, &file);
				if (wcscmp((wchar_t *)file.name, name) == 0)
				{
					found = true;
				}
				tinydir_next(&dir);
			}
			SHOULD_BE_TRUE(found);
		tinydir_close(&dir);
		_wremove(name);
		RemoveDirectoryW(TEST_PATH);
	SCENARIO_END
FEATURE_END

CBEHAVE_RUN("Windows unicode:", TEST_FEATURE(windows_unicode))
