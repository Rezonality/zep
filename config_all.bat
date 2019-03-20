set CURRENT_DIR=%CD%
mkdir build > nul
cd build
cmake -G "Visual Studio 15 2017 Win64" ..\
cd "%CURRENT_DIR%"

