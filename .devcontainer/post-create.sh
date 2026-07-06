#!/bin/bash
set -e

OPENCV_CMAKE_DIR=$(find /usr/lib -type d -name "opencv4" -path "*/cmake/*" 2>/dev/null | head -1)
if [[ -z "${OPENCV_CMAKE_DIR}" ]]; then
    echo "ERROR: opencv4 cmake directory not found" >&2
    exit 1
fi

rm -rf /workspace/build

cmake \
    -S /workspace \
    -B /workspace/build \
    -G Ninja \
    -DCMAKE_BUILD_TYPE=Debug \
    -DOpenCV_DIR="${OPENCV_CMAKE_DIR}"

cmake --build /workspace/build
