# ros2-cuda-tracker

![Status](https://img.shields.io/badge/status-in%20progress-yellow)

Real-time multi-object detection and tracking for ROS 2, targeting NVIDIA Jetson.
YOLOv10 inference on TensorRT, ByteTrack for cross-frame association.

Developed on a laptop NVIDIA GPU; deployed and benchmarked on a **Jetson Orin NX**
(Ampere, SM 8.7).

## Status

**Working today:** a ROS 2 Humble package `cuda_tracker` exposing one lifecycle node —
`Camera or /image_raw → Detector (TensorRT YOLOv10n FP16) → ByteTrack → /tracked_detections`.
Builds with `colcon`, runs from a V4L2 camera or an image topic, publishes
`cuda_tracker/msg/TrackedDetectionArray` with stable track IDs.

**No automated tests at present** — the suites were removed deliberately and are yet to
be reinstated.

**Not done yet:** the CUDA kernels and the INT8 engine. Everything below marked
*planned* is design, not code.

## Roadmap

The pipeline is built end-to-end first, then optimized in three rounds. Each round's
measurements are the baseline for the next.

| Round | Focus | Pipeline |
|---|---|---|
| **1 — CPU baseline** *(done)* | Working end-to-end pipeline, wrapped in a ROS 2 lifecycle node | `Camera → TensorRT FP16 → YOLOv10 parser → ByteTrack → ROS 2 node → Jetson` |
| **2 — CUDA GPU-first** *(planned)* | Custom CUDA kernels for pre/post-processing and tracker internals; pixels stay on GPU after H2D | `Camera → CUDA preprocess → TRT FP16 → CUDA postprocess → GPU ByteTrack → D2H (results only)` |
| **3 — INT8 + streams** *(planned)* | INT8 engine plus triple-buffered stream pipelining | Overlapped capture / infer / publish via `cudaStream_t` + `cudaEvent_t` |

## Quickstart

Requires Ubuntu 22.04, ROS 2 Humble, CUDA, TensorRT, and OpenCV.

```bash
# Dependencies (ros-humble-desktop or ros-base already covers these)
source /opt/ros/humble/setup.bash

# TensorRT engines are SM-architecture-specific and are never committed.
# Build one from the tracked ONNX on the machine that will run it. It lands in
# the repo under models/engine/, which is gitignored.
mkdir -p models/engine
trtexec --onnx=models/onnx/yolov10n.onnx --fp16 \
        --saveEngine=models/engine/yolov10n_fp16.engine

# Build
colcon build --base-paths ros2_ws --build-base ros2_ws/build \
             --install-base ros2_ws/install --symlink-install

# Run
source ros2_ws/install/setup.bash
ros2 launch cuda_tracker tracker.launch.py
```

`trtexec` ships with `libnvinfer-bin`; on some installs it is at
`/usr/src/tensorrt/bin/trtexec` rather than on `PATH`.

The launch file brings the node up and transitions it to `active` automatically. To
drive the lifecycle by hand instead:

```bash
ros2 run cuda_tracker cuda_tracker_node
ros2 lifecycle set /tracker_node configure
ros2 lifecycle set /tracker_node activate
```

### Topics

| Topic | Type | Direction |
|---|---|---|
| `/tracked_detections` | `cuda_tracker/msg/TrackedDetectionArray` | **published** — the node's only output |
| `/image_raw` | `sensor_msgs/Image` | **subscribed**, in `topic` mode only |

The node never publishes images. Frames are consumed and turned into tracks; in `camera`
mode nothing is republished, so there is no image traffic on the graph at all.

### Message

The package defines its own minimal message rather than using
`vision_msgs/Detection2DArray`, whose `Detection2D` nests a 6-DoF pose and a 36-element
covariance per detection that a 2D pixel-space tracker never fills in.

```
# cuda_tracker/msg/TrackedDetectionArray
std_msgs/Header      header       # stamp of the frame the tracks came from
TrackedDetection[]   detections

# cuda_tracker/msg/TrackedDetection
float32 center_x     # pixel coordinates in the source image
float32 center_y
float32 width
float32 height
int32   track_id     # stable while the tracker holds the object
int32   class_id     # COCO index; 0 is person
```

Detection confidence is deliberately not on the wire — add a `float32 score` field to
`msg/TrackedDetection.msg` if a consumer needs it; the pipeline already carries it
internally.

Input mode is a parameter, not a separate node. The launch file takes a single
`params_file` argument, so switching modes means pointing it at a copy of the YAML with
`input_source: "topic"`:

```bash
ros2 launch cuda_tracker tracker.launch.py params_file:=/path/to/topic_params.yaml
```

### Parameters

Every parameter and its default is documented in
`ros2_ws/src/cuda_tracker/config/tracker_params.yaml`. Override individually on the
command line:

```bash
ros2 run cuda_tracker cuda_tracker_node --ros-args \
  -p camera.device_id:=0 -p target_class_id:=-1
```

### Inspecting the output

The node is headless — it ships no visualization and no rviz config.

```bash
ros2 topic hz /tracked_detections
ros2 topic echo /tracked_detections --once
```

Rendering tracks means writing a consumer that subscribes to `/tracked_detections` and
draws the boxes against its own image source. There is no off-the-shelf display: rviz
has no plugin for a custom message, and Humble ships none for
`vision_msgs/Detection2DArray` either (`vision_msgs_rviz_plugins` is 3D-only), so this
was never a capability the custom message gave up.

## Architecture

One lifecycle node owns the whole pipeline and emits tracks only. Nothing renders and
nothing republishes images, which keeps frames from crossing a process boundary before
the CUDA work of Round 2.

```
                ┌──────────────────────────────────────────────┐
 V4L2 camera ──►│  TrackerNode  (LifecycleNode + component)     │
(input=camera)  │    Camera → Detector(Engine) → ObjectTracker  │──► /tracked_detections
                │    OR                                        │
 /image_raw ───►│    sub sensor_msgs/Image → cv_bridge          │
(input=topic)   └──────────────────────────────────────────────┘
```

All application code lives in namespace `vision`; the vendored tracker stays in
`byte_track`.

| Component | Source | Role |
|---|---|---|
| `TrackerNode` | `src/node/tracker_node.cpp` | Lifecycle node: parameters, both input paths, publishers |
| `Camera` | `src/camera/camera.cpp` | Wraps `cv::VideoCapture`; discards warmup frames |
| `TensorRtEngine` | `src/inference/engine.cpp` | Loads `.engine`, owns GPU buffers + `cudaStream_t`, runs `enqueueV3` |
| `Detector` | `src/inference/detector.cpp` | Letterbox preprocess (CPU→H2D), threshold + decode postprocess (D2H→CPU) |
| `ObjectTracker` | `src/tracker/tracker.cpp` | Adapter over vendored `byte_track::ByteTracker`, configured by `TrackerConfig` |
| conversions | `src/utils/conversions.cpp` | `TrackedDetection[]` → `cuda_tracker/msg/TrackedDetectionArray` |
| `Logger` | `src/utils/logger.cpp` | Level-filtered logging; `LOG_*` macros |

```
ros2_ws/src/cuda_tracker/
  package.xml  CMakeLists.txt  cmake/FindTensorRT.cmake
  msg/                TrackedDetection.msg, TrackedDetectionArray.msg
  include/
    node/             tracker_node.hpp
    camera/  inference/  tracker/ (+ bytetrack/, vendored)
    utils/            logger.hpp, conversions.hpp, detection_types.hpp
  src/
    node/             tracker_node.cpp
    camera/  inference/  tracker/
    utils/            logger.cpp, conversions.cpp
  launch/  config/
```

`utils/detection_types.hpp` holds `Detection` and `TrackedDetection` on their own so the
message conversions and the tracker adapter can be compiled and tested without pulling
in `NvInfer.h`.

### Lifecycle

The engine is loaded in `on_configure` and the camera opened in `on_activate`, so a
configured-but-inactive node holds GPU memory but no capture device. `on_cleanup`
releases both, destroying the detector last so nothing is mid-`Detect` against buffers
it owns. The CUDA primary context itself is only returned when the process exits —
that is CUDA's behaviour, not a leak; repeated configure/cleanup cycles return to the
same baseline.

### Logging

Nothing writes to `std::cout` / `std::cerr` directly. Every diagnostic — including
TensorRT's own — goes through `LOG_DEBUG/INFO/WARN/ERROR/FATAL(tag)`:

```cpp
#include "utils/logger.hpp"

LOG_INFO("Camera") << "device " << device_id << " | " << width << "x" << height;
```

Records go to stderr, which is unit-buffered and so is what `docker logs` captures
immediately; node-level messages use the ROS-native `RCLCPP_*` macros and reach
`/rosout` as usual. `LOG_LEVEL=debug|info|warn|error|fatal` overrides the default
(`info`) at startup, so verbosity is a container env var rather than a rebuild.

Two CMake targets: `bytetrack` (vendored tracker, no GPU dependencies) and
`tracker_node` (the whole pipeline as a component library, which also produces the
`cuda_tracker_node` executable).

Every CUDA call goes through `ThrowOnCudaError` (`include/inference/engine.hpp`).
Construction either yields a working object or throws — there are no `IsInitialized()`
probes.

### Tracker

Association uses a vendored port of
[ByteTrack-cpp](https://github.com/Vertical-Beach/ByteTrack-cpp) (MIT), reformatted to
this repository's style under `src/tracker/bytetrack/` and `include/tracker/bytetrack/`
(MIT text in `src/tracker/bytetrack/LICENSE`). The `ObjectTracker` adapter exposes the
association defaults through `TrackerConfig`, which the node fills from its `tracker.*`
parameters.

`src/tracker/bytetrack/lapjv.cpp` is the upstream Jonker-Volgenant solver, kept verbatim
as a numerical kernel — do not hand-edit it. Everything around it was restructured, and
with the test suites removed nothing currently guards that behaviour.

The detector's confidence threshold defaults to `0.1`. That is deliberately low:
ByteTrack's second association pass consumes the 0.1–0.5 band, so it is the tracking
floor, not a display filter.

ByteTrack's Kalman filter needs Eigen (header-only). The build prefers a system
`find_package(Eigen3)` and falls back to a pinned `FetchContent` drop, because the
TensorRT base images used for deployment do not ship Eigen.

## Dependencies

- ROS 2 Humble, `colcon`, `ament_cmake`
- CMake ≥ 3.17
- C++17-capable compiler
- OpenCV
- NVIDIA GPU with CUDA + TensorRT
- Eigen 3.3+ (falls back to a pinned source drop if absent)
- `cv_bridge` (the package generates its own messages, so no `vision_msgs`)
- A connected V4L2 camera (only for `input_source: "camera"`)

## Model

Models and engines live in the repository under `models/` — nothing is installed to
`/opt` or anywhere else on the system.

| Path | Committed? | What |
|---|---|---|
| `models/onnx/yolov10n.onnx` | yes | The portable artifact — **exported at 800×800** |
| `models/engine/*.engine` | no (gitignored) | Built locally by `trtexec` |

`model_input_size` must equal the engine's input tensor — 800 for the bundled ONNX, not
the more usual 640. The node reads the engine's real input dimensions and **refuses to
configure** on a mismatch, because a too-small letterbox is a short `cudaMemcpy` into a
larger buffer: it succeeds, and inference silently returns no detections. The engine logs
`input [WxH]` at construction if you need to check.

The default `engine_path` is `models/engine/yolov10n_fp16.engine`. **Relative paths
resolve against the repository root, not the process cwd** — the node bakes the repo
path in at build time, so `ros2 run` and `ros2 launch` work from any directory. Pass an
absolute `engine_path` to point outside the repo.

Engines are SM-architecture-specific and are **not** committed. Build one on the target
machine as shown in the Quickstart.

## Build

### Devcontainer (recommended)

Mounts `/dev` and the X11 socket so camera and display work out of the box, and builds
the workspace on creation. Open the repository in VS Code and choose
**Reopen in Container**, then:

```bash
source /workspace/ros2_ws/install/setup.bash
ros2 launch cuda_tracker tracker.launch.py
```

### Manual

See the Quickstart above. For sanitizers:

```bash
colcon build --base-paths ros2_ws --build-base ros2_ws/build \
             --install-base ros2_ws/install --symlink-install \
             --cmake-args -DCMAKE_CXX_FLAGS="-fsanitize=address,undefined"
```

## Tests

There are none right now. `colcon test` reports `0 tests` — the package declares no
suites, so a green run means nothing was checked.

When suites return, the convention worth keeping is that each test binary links only
what it exercises, so anything not testing inference stays free of CUDA and TensorRT and
runs on a GPU-less machine.

## License

Vendored ByteTrack sources are MIT-licensed; see `src/tracker/bytetrack/LICENSE`.

## References

[1] [ByteTrack-cpp](https://github.com/Vertical-Beach/ByteTrack-cpp) — C++ ByteTrack (current tracker)

[2] [sort-cpp](https://github.com/yasenh/sort-cpp) — C++ SORT (initial tracker, replaced)
