#!/bin/bash
set -e

if ! grep -q '/opt/ros/humble/setup.bash' "${HOME}/.bashrc"; then
    echo "source /opt/ros/humble/setup.bash" >> "${HOME}/.bashrc"
fi
# Source the workspace overlay too, so `ros2 launch cuda_tracker ...` works in a
# fresh shell without the user having to remember it.
if ! grep -q '/workspace/ros2_ws/install/setup.bash' "${HOME}/.bashrc"; then
    echo "[ -f /workspace/ros2_ws/install/setup.bash ] && source /workspace/ros2_ws/install/setup.bash" \
        >> "${HOME}/.bashrc"
fi

source /opt/ros/humble/setup.bash

colcon build --base-paths /workspace/ros2_ws \
             --build-base /workspace/ros2_ws/build \
             --install-base /workspace/ros2_ws/install \
             --symlink-install
