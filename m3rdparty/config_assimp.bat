set CURRENT_DIR=%CD%
cd assimp
mkdir bin
cd bin
cmake -G "Visual Studio 15 2017 Win64" -DASIMP_BUILD_TOOLS=OFF -DASSIMP_BUILD_SAMPLES=OFF -DENABLE_BOOST_WORKAROUND=ON -DLIBRARY_SUFFIX="" -DASSIMP_BUILD_TESTS=OFF ..\
cmake --build . --target ALL_BUILD --config Release -- /nologo
cmake --build . --target ALL_BUILD --config Debug -- /nologo

set "%CURRENT_DIR%"