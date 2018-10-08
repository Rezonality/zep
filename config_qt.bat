set CURRENT_DIR=%CD%
mkdir build > nul
cd build


cmake -G "Visual Studio 15 2017 Win64" -DBUILD_QT=YES -DBUILD_IMGUI=NO ..\
cd "%CURRENT_DIR%"

