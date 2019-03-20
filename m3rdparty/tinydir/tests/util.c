#include <stdio.h>

#ifdef _MSC_VER
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <stdlib.h>
#include <unistd.h>
#endif


void make_temp_file(const char *prefix, char *out)
{
#ifdef _MSC_VER
	if (GetTempFileName(".", prefix, 0, out) != 0)
	{
		// Strip the ".\\" prefix
		if (strncmp(out, ".\\", 2) == 0)
		{
			memmove(out, out + 2, strlen(out));
		}
		// Create file
		fclose(fopen(out, "w"));
	}
#else
	sprintf(out, "%sXXXXXX", prefix);
	close(mkstemp(out));
#endif
}
