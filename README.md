# ros2-cuda-tracker

![Status](https://img.shields.io/badge/status-ongoing-yellow)

Real-time multi-object detection and tracking for ROS 2, targeting NVIDIA Jetson.
The pipeline runs YOLOv10 inference on TensorRT and associates detections across
frames with ByteTrack, exposing camera, detection, and visualization stages as
ROS 2 lifecycle nodes.

Developed on a laptop NVIDIA GPU; deployed and benchmarked on a **Jetson Orin NX**
(Ampere, SM 8.7).

## Overview

The pipeline is built end-to-end first, then optimized in three rounds. Each round's
measurements form the baseline for the next. See [`docs/ROADMAP.md`](docs/ROADMAP.md)
for the full stage-by-stage plan and [`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md)
for design detail.

| Round | Focus | Pipeline |
|---|---|---|
| **1 — CPU baseline** | Working end-to-end pipeline | `Camera → TensorRT FP16 → YOLOv10 parser → ByteTrack → ROS 2 nodes → Jetson` |
| **2 — CUDA GPU-first** | Custom CUDA kernels for pre/post-processing + tracker internals; pixels stay on GPU after H2D | `Camera → CUDA preprocess → TRT FP16 → CUDA postprocess → GPU ByteTrack → D2H (results only)` |
| **3 — INT8 + streams** | INT8 engine + triple-buffered CUDA stream pipelining | Overlapped capture / infer / publish via `cudaStream_t` + `cudaEvent_t` |

## Architecture

Round 1 core classes are implemented; ROS 2 node wrappers are in progress.

| Component | Source | Role |
|---|---|---|
| `Camera` | `src/camera.cpp` | Wraps `cv::VideoCapture`; EMA FPS estimate |
| `Engine` | `src/engine.cpp` | Loads `.engine` file, owns GPU buffers + `cudaStream_t`, runs `enqueueV3` |
| `Detector` | `src/detector.cpp` | Letterbox preprocess (CPU→H2D), threshold + decode postprocess (D2H→CPU) |
| `ByteTracker` | `src/tracker.cpp` | Adapter over vendored `byte_track::ByteTracker`; returns `TrackedDetection[]` with `track_id` |
| `utils` | `src/utils.cpp` | Overlay helpers (`DrawTrackedDetections`, `DrawFps`) |

Current pipeline: `Camera → Detector (TensorRT YOLOv10x FP16) → ByteTracker → Visualizer`.

### Tracker

Association uses a vendored port of
[ByteTrack-cpp](https://github.com/Vertical-Beach/ByteTrack-cpp) (MIT), reformatted to
this repository's Google C++ style under `include/bytetrack/` and `src/bytetrack/`. The
public `ByteTracker` adapter (`src/tracker.cpp`) is tuned via constructor arguments:
`frame_rate`, `track_buffer`, `track_thresh`, `high_thresh`, `match_thresh`. ByteTrack
returns only activated tracks, so every `TrackedDetection` carries a live
`class_id`/`confidence`.

ByteTrack's Kalman filter depends on Eigen (header-only). CMake prefers a system
`find_package(Eigen3)` and falls back to a pinned `3.4.0` `FetchContent` if absent —
e.g. inside the TensorRT devcontainer, which does not ship Eigen. The vendored sources
compile into a static `bytetrack_lib`.

### Planned ROS 2 nodes

Three lifecycle nodes communicating over topics:

| Node | Publishes | Subscribes |
|---|---|---|
| `CameraNode` | `/camera/image_raw` | — |
| `DetectionNode` (TensorRT + ByteTrack) | `/detections` | `/camera/image_raw` |
| `VisualizerNode` | `/annotated_image` | `/camera/image_raw`, `/detections` |

## Dependencies

- CMake ≥ 3.22, Ninja
- C++23-capable compiler
- OpenCV
- CUDA + TensorRT
- Eigen 3.4 (system or auto-fetched)
- NVIDIA GPU with CUDA and TensorRT installed
- A connected V4L2 camera

## Model

The pipeline requires a TensorRT engine at `models/engine/yolov10x_fp16.engine`.
Engines are SM-architecture-specific — build on the target machine:

```bash
trtexec --onnx=models/yolov10x.onnx \
        --saveEngine=models/engine/yolov10x_fp16.engine \
        --fp16
```

The portable artifact is `yolov10x.onnx` — commit that, not the engine.

## Build

### Devcontainer (recommended)

The devcontainer mounts `/dev` and the X11 socket so the camera and display work out of
the box, and runs `cmake` automatically on creation.

1. Open the repository in VS Code.
2. When prompted, click **Reopen in Container** (or run `Dev Containers: Reopen in Container`).
3. Run the binary:

```bash
./build/bin/Camera
```

### Manual build

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build
./build/bin/Camera
```

To build with sanitizers, add `-DCMAKE_CXX_FLAGS="-fsanitize=address,undefined"` at
configure time.

## Configuration

Setup-specific constants in `src/main.cpp`:

| Constant | Default | Meaning |
|---|---|---|
| `kDefaultDeviceId` | `4` | V4L2 camera index (`/dev/videoX`) |
| `kEnginePath` | `models/engine/yolov10x_fp16.engine` | Engine path, relative to repo root |
| `kPersonClassId` | `0` | COCO class filter (`-1` accepts all classes) |

## Usage

Run from the repository root — the engine path is relative:

```bash
./build/bin/Camera
```

| Key | Action |
|-----|--------|
| `q` / `ESC` | Quit |

## License

The vendored ByteTrack sources are MIT-licensed; see `third_party/ByteTrack-cpp.LICENSE`.

## References

[1] [ByteTrack-cpp](https://github.com/Vertical-Beach/ByteTrack-cpp) — C++ implementation of ByteTrack (Round 1 pipeline tracker)

[2] [sort-cpp](https://github.com/yasenh/sort-cpp) — C++ implementation of SORT (initial tracker, since replaced)
