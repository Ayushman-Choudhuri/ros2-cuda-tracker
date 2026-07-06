# ros2-deepstream-object-tracking

![Status](https://img.shields.io/badge/status-ongoing-yellow)

## 1.0 Goal

Real-time object detection and tracking pipeline targeting NVIDIA Jetson hardware.
Combines YOLOv10x (TensorRT FP16) with SORT tracking, evolving into a zero-copy GStreamer DeepStream pipeline over ROS2 LifecycleNodes.

**Current pipeline:** `Camera → Detector (TensorRT YOLOv10x FP16) → SORTTracker → Visualizer`

## 2.0 Setup

### Prerequisites

- Docker + Docker Compose
- VS Code with the [Dev Containers](https://marketplace.visualstudio.com/items?itemName=ms-vscode-remote.remote-containers) extension
- NVIDIA GPU with CUDA and TensorRT installed
- A connected camera (update `kDefaultDeviceId` in `src/main.cpp` to match your `/dev/videoX`)

### Model

The pipeline requires a TensorRT engine file at `models/engine/yolov10x_fp16.engine`.
Engines are SM-architecture-specific — build on the target machine:

```bash
trtexec --onnx=models/yolov10x.onnx \
        --saveEngine=models/engine/yolov10x_fp16.engine \
        --fp16
```

The portable artifact is `yolov10x.onnx` — commit that, not the engine.

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

Requires: CMake ≥ 3.22, Ninja, OpenCV, TensorRT, CUDA, a C++23-capable compiler.

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

## 3.0 References

[1] [DeepStream-SDK-Notebook](https://github.com/kimsooyoung/DeepStream-SDK-Notebook)

[2] [sort-cpp](https://github.com/yasenh/sort-cpp) — C++ implementation of SORT (Simple Online and Realtime Tracking)
