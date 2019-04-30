set CURRENT_DIR=%CD%
mkdir build > nul
cd build
cmake -G "Visual Studio 16 2019" -A x64 -DZEP_FEATURE_CPP_FILE_SYSTEM=1 ..\
cd "%CURRENT_DIR%"

