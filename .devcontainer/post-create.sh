#!/bin/bash
set -e

rm -rf /workspace/build

cmake \
    -S /workspace \
    -B /workspace/build \
    -G Ninja \
    -DCMAKE_BUILD_TYPE=Debug \
    -DOpenCV_DIR=/usr/lib/x86_64-linux-gnu/cmake/opencv4

cmake --build /workspace/build
