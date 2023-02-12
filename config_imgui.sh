#!/bin/sh

mkdir build
cd build
if [ "$1" != "" ] ; then
cmake -G "Unix Makefiles" -DBUILD_QT=OFF -DBUILD_IMGUI=ON -DCMAKE_BUILD_TYPE=$1 ../
else
cmake -G "Unix Makefiles" -DBUILD_QT=OFF -DBUILD_IMGUI=ON -DBUILD_EXTENSIONS=ON -DCMAKE_BUILD_TYPE=Release ../
fi
cd ../
