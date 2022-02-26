set CURRENT_DIR=%CD%
mkdir build > nul
cd build
cmake -G "Visual Studio 17 2022" ..\
cd "%CURRENT_DIR%"

