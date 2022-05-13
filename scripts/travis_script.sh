#!/bin/sh

if test "${BUILD_TYPE}" = "Coverity"; then
  git clone https://github.com/cmaughan/MUtils
  cd MUtils
  chmod +x prebuild.sh
  chmod +x m3rdparty/packages/install.sh
  ./prebuild.sh
  mkdir build
  cd build
  cmake .. \
    -DCMAKE_C_COMPILER=${CMAKE_C_COMPILER} \
    -DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER}
  cmake --build . --target install
  cd ..
  cd ..
  mkdir build
  cd build
  cmake .. \
    -DCMAKE_C_COMPILER=${CMAKE_C_COMPILER} \
    -DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER} \
    -DPROJECT_UNITTESTS=no
  bash <(curl -s https://scan.coverity.com/scripts/travisci_build_coverity_scan.sh)
else
  git clone https://github.com/cmaughan/MUtils
  cd MUtils
  chmod +x prebuild.sh
  chmod +x m3rdparty/packages/install.sh
  ./prebuild.sh
  mkdir build
  cd build
  cmake .. \
    -DCMAKE_C_COMPILER=${CMAKE_C_COMPILER} \
    -DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER}
  cmake --build . --target install
  cd ..
  cd ..
  mkdir build
  cd build
  cmake .. \
    -DCMAKE_C_COMPILER=${CMAKE_C_COMPILER} \
    -DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER} \
    -DPROJECT_UNITTESTS=no
  cmake --build . --target ${TEST_TARGET} -- ARGS=--output-on-failure
fi
