@echo off


if not exist "vcpkg\" (
    echo Make sure you add the submodule: 'git submodule update --init'!
    exit 1
)

echo %Time%

if not exist "vcpkg\vcpkg.exe" (
    cd vcpkg
    call bootstrap-vcpkg.bat -disableMetrics
    cd %~dp0
)

echo Installing libraries for the demo...
cd vcpkg
echo Installing Libraries
vcpkg install clipp tinydir fmt imgui[freetype,sdl2-binding,opengl3-binding] tinyfiledialogs gl3w gsl-lite concurrentqueue catch2 cpptoml freetype sdl2 --triplet x64-windows-static-md --recurse
cd %~dp0

echo %Time%

