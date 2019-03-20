set CURRENT_DIR=%CD%
cd flatbuffers
mkdir bin
cd bin
cmake -G "Visual Studio 15 2017 Win64" ..\
cmake --build . --target ALL_BUILD --config Release -- /nologo

set "%CURRENT_DIR%"