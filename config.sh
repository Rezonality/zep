#!/bin/sh

mkdir build
cd build
if [ "$1" != "" ] ; then
cmake -G "Unix Makefiles" -DBUILD_QT=ON -DBUILD_IMGUI=ON -DCMAKE_BUILD_TYPE=$1 ../
else
cmake -G "Unix Makefiles" -DBUILD_QT=ON -DBUILD_IMGUI=ON -DCMAKE_BUILD_TYPE=Release ../
fi
cd ../
