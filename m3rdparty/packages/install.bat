set CURRENT_DIR=%CD%
mkdir build > nul
cd build
cmake -G "Visual Studio 16 2019" -A x64 ..\
cmake --build .
cmake --build . --config Release
cd %CURRENT_DIR%

