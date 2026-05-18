# ros2-deepstream-object-tracking

## 1.0 Goal

Real-time object detection and tracking pipeline targeting NVIDIA Jetson hardware.
Combines YOLOv10n (TensorRT FP16) with DeepSORT tracking over ROS2 LifecycleNodes, evolving into a zero-copy GStreamer DeepStream pipeline.

## 2.0 Setup

### Prerequisites

- Docker + Docker Compose
- VS Code with the [Dev Containers](https://marketplace.visualstudio.com/items?itemName=ms-vscode-remote.remote-containers) extension
- A connected camera (update `DEFAULT_DEVICE_ID` in `src/main.cpp` to match your `/dev/videoX`)

### Devcontainer (recommended)

The devcontainer mounts `/dev` and the X11 socket so the camera and display work out of the box.

1. Open the repo in VS Code.
2. When prompted, click **Reopen in Container** (or run `Dev Containers: Reopen in Container` from the command palette).
3. The container runs `cmake` automatically on creation — the binary is ready at `build/bin/Camera`.

```bash
# Run
./build/bin/Camera
```

### Manual build

Requires: CMake ≥ 3.22, Ninja, OpenCV, a C++23-capable compiler.

```bash
# Configure
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug

# Build
cmake --build build

# Run
./build/bin/Camera
```

### Controls

| Key | Action |
|-----|--------|
| `q` / `ESC` | Quit |
| `s` | Save current frame to disk |

## 3.0 References

[1] [DeepStream-SDK-Notebook](https://github.com/kimsooyoung/DeepStream-SDK-Notebook)
