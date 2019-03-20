#!/bin/sh

if test "${BUILD_TYPE}" = "Coverage"; then
  coveralls-lcov ${TRAVIS_BUILD_DIR}/build/coverage.info
  cd ${TRAVIS_BUILD_DIR}/build
  bash <(curl -s https://codecov.io/bash) || echo "Codecov did not collect coverage reports"
fi