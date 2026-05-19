#!/bin/bash
set -e

cmake \
    -S /workspace \
    -B /workspace/build \
    -G Ninja \
    -DCMAKE_BUILD_TYPE=Debug

cmake --build /workspace/build
