#!/bin/bash -x
# Remember to preinstall linux libraries:
# (ibus,  tar, zip, unzip, buid-prerequisits, xorg-dev)

if [ ! -d "../vcpkg" ]; then
    git clone --single-branch --branch master https://github.com/Microsoft/vcpkg.git ../vcpkg
fi

if [ ! -f "../vcpkg/vcpkg" ]; then
    cd ../vcpkg
    ./bootstrap-vcpkg.sh -disableMetrics
    cd ../mutils
fi

triplet=(x64-linux)
if [ "$(uname)" == "Darwin" ]; then
   triplet=(x64-osx)
fi

if [ ! -d "../vcpkg/imgui" ]; then
  cd ../vcpkg
  ./vcpkg install kissfft cppcodec clipp date kubazip tinydir fmt portaudio nanovg libcuckoo foonathan-memory cpp-httplib tinyfiledialogs imgui[sdl2-binding,freetype,opengl3-binding] gl3w gsl-lite glm concurrentqueue utfcpp stb catch2 magic-enum nlohmann-json cpptoml toml11 drlibs freetype sdl2 --triplet ${triplet[0]} --recurse
  if [ "$(uname)" != "Darwin" ]; then
  ./vcpkg install glib --triplet ${triplet[0]} --recurse
  fi
  cd ../mutils
fi

