#!/bin/bash
mkdir build > nul
pushd ./build
cmake -G "Unix Makefiles" ../
sudo make
popd

