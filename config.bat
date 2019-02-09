set CURRENT_DIR=%CD%
mkdir build > nul
cd build
cmake -G "Visual Studio 15 2017 Win64" -DZEP_FEATURE_CPP_FILE_SYSTEM=1 -DZEP_FEATURE_FILE_WATCHER=1 ..\
cd "%CURRENT_DIR%"

