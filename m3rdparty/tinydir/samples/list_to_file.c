#include <stdio.h>

#define _UNICODE
#define UNICODE
#include "tinydir.h"

char bom[] = "\xFF\xFE";

int main(void) {

  FILE *fp;
  tinydir_dir dir;
  tinydir_open(&dir, TINYDIR_STRING("/path/to/dir"));

  fp = 
#if ((defined _WIN32) && (defined _UNICODE))
	_wfopen(
#else
	fopen(
#endif
		TINYDIR_STRING("/file/to/output"), TINYDIR_STRING("wb"));

#if ((defined _WIN32) && (defined _UNICODE))
  fwrite(bom, 1, 2, fp);
#endif

  while (dir.has_next)
  {
    tinydir_file file;
    tinydir_readfile(&dir, &file);

    fwrite(file.name, sizeof(_tinydir_char_t), _tinydir_strlen(file.name), fp);
    if (file.is_dir)
    {
        fwrite(TINYDIR_STRING("/"), sizeof(_tinydir_char_t), _tinydir_strlen(TINYDIR_STRING("/")), fp);
    }
    fwrite(TINYDIR_STRING("\n"), sizeof(_tinydir_char_t), _tinydir_strlen(TINYDIR_STRING("/")), fp);

    tinydir_next(&dir);
  }

  tinydir_close(&dir);

  fclose(fp);

  return 0;
}
