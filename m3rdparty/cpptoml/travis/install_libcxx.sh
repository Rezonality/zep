#!/bin/bash
set -v
cwd=$(pwd)

LLVM_TAG="${LLVM_TAG:-RELEASE_381}"

svn co --quiet http://llvm.org/svn/llvm-project/llvm/tags/$LLVM_TAG/final llvm

cd llvm/projects
svn co --quiet http://llvm.org/svn/llvm-project/libcxx/tags/$LLVM_TAG/final libcxx
svn co --quiet http://llvm.org/svn/llvm-project/libcxxabi/tags/$LLVM_TAG/final libcxxabi
cd ../

mkdir build
cd build
cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=$HOME ../
make cxx
make install-libcxx install-libcxxabi

cd $cwd
set +v
