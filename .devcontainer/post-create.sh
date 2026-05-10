#!/bin/bash
set -e

cmake \
    -S /workspace \
    -B /workspace/build \
    -G Ninja \
    -DCMAKE_BUILD_TYPE=Debug \
    -DCMAKE_EXPORT_COMPILE_COMMANDS=ON

ln -sf /workspace/build/compile_commands.json /workspace/compile_commands.json
