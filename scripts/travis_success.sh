if test "${BUILD_TYPE}" = "Coverage"; then
  coveralls-lcov ${TRAVIS_BUILD_DIR}/build/coverage.info
fi