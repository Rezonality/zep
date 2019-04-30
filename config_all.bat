set CURRENT_DIR=%CD%
mkdir build > nul
cd build
cmake -G "Visual Studio 16 2019" -A x64 -DBUILD_QT=YES -DBUILD_IMGUI=YES ..\
cd "%CURRENT_DIR%"

