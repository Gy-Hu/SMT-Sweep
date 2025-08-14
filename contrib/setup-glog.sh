#!/bin/bash

GLOG_VERSION=v0.6.0

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"
DEPS=$DIR/../deps

mkdir -p $DEPS

if [ ! -d "$DEPS/glog" ]; then
    cd $DEPS
    git clone https://github.com/google/glog.git glog
    cd glog
    git checkout -f $GLOG_VERSION
    mkdir -p "$DEPS/glog/install"
    cmake -DCMAKE_INSTALL_PREFIX="$DEPS/glog/install" -S . -B build -G "Unix Makefiles" \
        -DBUILD_SHARED_LIBS=OFF \
        -DWITH_GFLAGS=OFF \
        -DWITH_UNWIND=none \
        -DWITH_SYMBOLIZE=OFF \
        -DWITH_STACKTRACE=OFF \
        -DGTEST_HAS_UNWIND=0 \
        -DUnwind_FOUND=FALSE \
        -DUnwind_LIBRARY="" \
        -DUnwind_INCLUDE_DIR="" \
        -DCMAKE_CXX_FLAGS="-static -DHAVE_NO_SYMBOLIZE=1 -DHAVE_NO_STACKTRACE=1 -DNO_FRAME_POINTER -DGOOGLE_STRIP_LOG=1" \
        -DCMAKE_C_FLAGS="-static" \
        -DCMAKE_EXE_LINKER_FLAGS="-static" \
        -DCMAKE_FIND_LIBRARY_SUFFIXES=".a"
    cmake --build build
    cd build
    make install
else
    echo "$DEPS/glog already exists. If you want to rebuild, please remove $DEPS/glog and start over."
fi