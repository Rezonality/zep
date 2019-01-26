#!/bin/sh

mkdir build
cd build
cmake -G "Unix Makefiles" -DBUILD_QT=ON -DBUILD_IMGUI=ON ../
cd ../
