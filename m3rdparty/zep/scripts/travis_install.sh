#!/bin/sh

case "${TRAVIS_OS_NAME}-${CC}" in
  linux-clang)
    ;;

  linux-gcc)
    if test "${BUILD_TYPE}" = "Coverage"; then
      curl -sO http://ftp.debian.org/debian/pool/main/l/lcov/lcov_1.13.orig.tar.gz
      gunzip -c lcov_1.13.orig.tar.gz | tar xvf -
      make -C lcov-1.13 install PREFIX=${HOME} BIN_DIR=${HOME}/bin
      gem install coveralls-lcov
    fi
    ;;

  osx-clang)
    brew update
    brew install cmake || true
    ;;

  osx-gcc)
    brew update
    brew install cmake gcc || true
    ;;
esac
