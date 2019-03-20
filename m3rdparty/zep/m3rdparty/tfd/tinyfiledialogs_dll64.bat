\mingw-w64\mingw64\bin\gcc -ansi -std=c89 -pedantic -m64 -c tinyfiledialogs.c
\mingw-w64\mingw64\bin\dlltool --export-all-symbols -l tinyfiledialogs64.lib tinyfiledialogs.o --dllname tinyfiledialogs64.dll
\mingw-w64\mingw64\bin\gcc -m64 -shared tinyfiledialogs.o -o tinyfiledialogs64.dll -L\mingw-w64\mingw64\lib -lcomdlg32 -lole32
\mingw-w64\mingw64\bin\gcc -ansi -std=c89 -pedantic -m64 -o hello.exe hello.c tinyfiledialogs64.lib

@REM \mingw-w64\mingw64\bin\gcc -o hello.exe tinyfiledialogs.c hello.c -LC:\mingw-w64\mingw64\lib -lcomdlg32 -lole32
