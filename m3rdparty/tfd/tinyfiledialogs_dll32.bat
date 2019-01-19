gcc -ansi -std=gnu89 -pedantic -c tinyfiledialogs.c
dlltool --export-all-symbols -l tinyfiledialogs32.lib tinyfiledialogs.o --dllname tinyfiledialogs32.dll
gcc -shared tinyfiledialogs.o -o tinyfiledialogs32.dll -LC:/mingw/lib -lcomdlg32 -lole32
gcc -ansi -std=c89 -pedantic -o hello.exe hello.c tinyfiledialogs32.lib

rem gcc -ansi -std=gnu89 -pedantic -c tinyfiledialogs.c
rem dlltool --export-all-symbols -l tinyfiledialogs32.lib tinyfiledialogs.o --dllname tinyfiledialogs32.dll
rem gcc -shared tinyfiledialogs.o -o tinyfiledialogs32.dll -LC:/mingw/lib -lcomdlg32 -lole32
rem gcc -ansi -std=c89 -pedantic -o hello.exe hello.c tinyfiledialogs32.lib
