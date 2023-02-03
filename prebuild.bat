@echo off

echo %Time%

if not exist "vcpkg\" (
  echo Download vcpkg from github
  git clone --single-branch --branch master https://github.com/Microsoft/vcpkg.git vcpkg
  if not exist "vcpkg\vcpkg.exe" (
    cd vcpkg
    call bootstrap-vcpkg.bat -disableMetrics
    cd %~dp0
  )
)

cd vcpkg
echo Installing Libraries
vcpkg install clipp tinydir fmt imgui[freetype,sdl2-binding,opengl3-binding] tinyfiledialogs gl3w gsl-lite concurrentqueue catch2 cpptoml toml11 freetype sdl2 --triplet x64-windows-static-md --recurse
cd %~dp0

echo %Time%

