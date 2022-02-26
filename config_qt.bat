set CURRENT_DIR=%CD%
mkdir build > nul
cd build

cmake -G "Visual Studio 17 2022 Win64" -DBUILD_QT=YES -DBUILD_IMGUI=NO ..\
cd "%CURRENT_DIR%"

