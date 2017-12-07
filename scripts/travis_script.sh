if test "${BUILD_TYPE}" = "Coverity"; then
  mkdir build
  cd build
  cmake .. \
    -DCMAKE_C_COMPILER=${CMAKE_C_COMPILER} \
    -DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER} \
    -DPROJECT_UNITTESTS=no
  bash <(curl -s https://scan.coverity.com/scripts/travisci_build_coverity_scan.sh)
else
  mkdir build
  cd build
  cmake .. \
    -DCMAKE_C_COMPILER=${CMAKE_C_COMPILER} \
    -DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER} \
    -DCMAKE_BUILD_TYPE=${BUILD_TYPE}
  cmake --build .
  cmake --build . --target ${TEST_TARGET} -- ARGS=--output-on-failure
fi