# ros2-cuda-tracker

![Status](https://img.shields.io/badge/status-in%20progress-yellow)

Real-time multi-object detection and tracking for ROS 2, targeting NVIDIA Jetson.
YOLOv10 inference on TensorRT, ByteTrack for cross-frame association.

Developed on a laptop NVIDIA GPU; deployed and benchmarked on a **Jetson Orin NX**
(Ampere, SM 8.7).

## Status

**Working today:** a standalone C++ pipeline —
`Camera → Detector (TensorRT YOLOv10x FP16) → ObjectTracker → overlays`.
Builds with CMake/Ninja, runs from a V4L2 camera, draws tracked boxes with stable IDs.
Tracking behaviour is covered by GoogleTest suites.

**Not done yet:** the ROS 2 lifecycle nodes, the CUDA kernels, the INT8 engine.
Everything below marked *planned* is design, not code.

## Roadmap

The pipeline is built end-to-end first, then optimized in three rounds. Each round's
measurements are the baseline for the next.

| Round | Focus | Pipeline |
|---|---|---|
| **1 — CPU baseline** *(in progress)* | Working end-to-end pipeline, wrapped in ROS 2 nodes | `Camera → TensorRT FP16 → YOLOv10 parser → ByteTrack → ROS 2 nodes → Jetson` |
| **2 — CUDA GPU-first** *(planned)* | Custom CUDA kernels for pre/post-processing and tracker internals; pixels stay on GPU after H2D | `Camera → CUDA preprocess → TRT FP16 → CUDA postprocess → GPU ByteTrack → D2H (results only)` |
| **3 — INT8 + streams** *(planned)* | INT8 engine plus triple-buffered stream pipelining | Overlapped capture / infer / publish via `cudaStream_t` + `cudaEvent_t` |

Round 1's three lifecycle nodes, once wrapped:

| Node | Publishes | Subscribes |
|---|---|---|
| `CameraNode` | `/camera/image_raw` | — |
| `DetectionNode` (TensorRT + ByteTrack) | `/detections` | `/camera/image_raw` |
| `VisualizerNode` | `/annotated_image` | `/camera/image_raw`, `/detections` |

## Architecture

All application code lives in namespace `vision`.

| Component | Source | Role |
|---|---|---|
| `Camera` | `src/camera/camera.cpp` | Wraps `cv::VideoCapture`; EMA FPS estimate |
| `TensorRtEngine` | `src/inference/engine.cpp` | Loads `.engine`, owns GPU buffers + `cudaStream_t`, runs `enqueueV3` |
| `Detector` | `src/inference/detector.cpp` | Letterbox preprocess (CPU→H2D), threshold + decode postprocess (D2H→CPU) |
| `ObjectTracker` | `src/tracker/tracker.cpp` | Adapter over vendored `byte_track::ByteTracker`; returns `TrackedDetection[]` with `track_id` |
| overlays | `src/utils/visualization.cpp` | `DrawTracks`, `DrawFps` |
| `Logger` | `src/utils/logger.cpp` | Level-filtered logging to stderr; `LOG_*` macros |

```
include/
  camera/           camera.hpp
  inference/        engine.hpp, detector.hpp
  tracker/          tracker.hpp, bytetrack/ (vendored)
  utils/            logger.hpp, visualization.hpp
src/
  main.cpp
  camera/  inference/  tracker/  utils/
tests/              GoogleTest suites
```

### Logging

Nothing writes to `std::cout` / `std::cerr` directly. Every diagnostic — including
TensorRT's own — goes through `LOG_DEBUG/INFO/WARN/ERROR/FATAL(tag)`:

```cpp
#include "utils/logger.hpp"

LOG_INFO("Camera") << "device " << device_id << " | " << width << "x" << height;
```

Records go to stderr, which is what `docker logs` captures and what ROS 2 logs to.
`LOG_LEVEL=debug|info|warn|error|fatal` overrides the default (`info`) at
startup, so verbosity is a container env var rather than a rebuild.

Three CMake targets: `bytetrack` (vendored tracker, no GPU dependencies), `vision`
(the pipeline), and the `tracker` executable.

Every CUDA call goes through `ThrowOnCudaError` (`include/inference/engine.hpp`).
Construction either yields a working object or throws — there are no `IsInitialized()`
probes.

### Tracker

Association uses a vendored port of
[ByteTrack-cpp](https://github.com/Vertical-Beach/ByteTrack-cpp) (MIT), reformatted to
this repository's style under `src/tracker/bytetrack/` and `include/tracker/bytetrack/`
(MIT text in `src/tracker/bytetrack/LICENSE`). The `ObjectTracker` adapter keeps the
association defaults declared in `byte_tracker.hpp`.

`src/tracker/bytetrack/lapjv.cpp` is the upstream Jonker-Volgenant solver, kept verbatim
as a numerical kernel — do not hand-edit it. Everything around it was restructured;
`tests/bytetrack_test.cpp` guards the behaviour.

ByteTrack's Kalman filter needs Eigen (header-only), found with `find_package(Eigen3)`.
The devcontainer installs `libeigen3-dev`.

## Dependencies

- CMake ≥ 3.22, Ninja
- C++23-capable compiler
- OpenCV
- NVIDIA GPU with CUDA + TensorRT
- Eigen 3.3+
- A connected V4L2 camera

## Model

The pipeline needs a TensorRT engine at `models/engine/yolov10x_fp16.engine`. Engines are
SM-architecture-specific, so build on the target machine:

```bash
trtexec --onnx=models/yolov10x.onnx \
        --saveEngine=models/engine/yolov10x_fp16.engine \
        --fp16
```

`yolov10x.onnx` is the portable artifact — commit that, not the engine.

## Build

### Devcontainer (recommended)

Mounts `/dev` and the X11 socket so camera and display work out of the box, and runs
`cmake` on creation. Open the repository in VS Code and choose **Reopen in Container**,
then:

```bash
./build/bin/tracker
```

### Manual

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build
ctest --test-dir build --output-on-failure
./build/bin/tracker
```

Add `-DCMAKE_CXX_FLAGS="-fsanitize=address,undefined"` at configure time for sanitizers.

## Tests

GoogleTest, registered with CTest.

```bash
ctest --test-dir build --output-on-failure   # all suites
./build/bin/bytetrack_test                   # one binary directly
./build/bin/bytetrack_test --gtest_filter='ByteTrackerTest.*'
```

Each test binary links only what it exercises, so `bytetrack_test` links `bytetrack`
alone and needs neither CUDA nor TensorRT.

## Usage

Run from the repository root — the engine path is relative.

```bash
./build/bin/tracker
```

Press `q` or `ESC` to quit.

Setup-specific constants in `src/main.cpp`:

| Constant | Default | Meaning |
|---|---|---|
| `kCameraDeviceId` | `4` | V4L2 camera index (`/dev/videoX`) |
| `kEnginePath` | `models/engine/yolov10x_fp16.engine` | Engine path, relative to repo root |
| `kPersonClassId` | `0` | COCO class filter (`Detector::kAllClasses` accepts all) |

## License

Vendored ByteTrack sources are MIT-licensed; see `src/tracker/bytetrack/LICENSE`.

## References

[1] [ByteTrack-cpp](https://github.com/Vertical-Beach/ByteTrack-cpp) — C++ ByteTrack (current tracker)

[2] [sort-cpp](https://github.com/yasenh/sort-cpp) — C++ SORT (initial tracker, replaced)
