cmake_minimum_required(VERSION 3.17)
project(ros2_cuda_tracker VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(OpenCV REQUIRED)
find_package(CUDAToolkit REQUIRED)

find_path(TENSORRT_INCLUDE_DIR NvInfer.h
    PATHS /usr/include/x86_64-linux-gnu /usr/include/aarch64-linux-gnu /usr/local/include /usr/include)
find_library(TENSORRT_LIBRARY nvinfer
    PATHS /usr/lib/x86_64-linux-gnu /usr/lib/aarch64-linux-gnu /usr/local/lib /usr/lib)

if(NOT TENSORRT_INCLUDE_DIR OR NOT TENSORRT_LIBRARY)
    message(FATAL_ERROR
        "TensorRT not found. Run inside the devcontainer or set "
        "TENSORRT_INCLUDE_DIR and TENSORRT_LIBRARY manually.")
endif()

find_package(Eigen3 3.3 REQUIRED NO_MODULE)

message(STATUS "TensorRT include : ${TENSORRT_INCLUDE_DIR}")
message(STATUS "TensorRT library : ${TENSORRT_LIBRARY}")
message(STATUS "OpenCV version   : ${OpenCV_VERSION}")

# Vendored ByteTrack-cpp port (MIT, see src/tracker/bytetrack/LICENSE). Kept as
# its own target so tests can exercise the tracker without CUDA or TensorRT.
# `include/tracker` is on its include path so the vendored sources keep spelling
# their own headers the way upstream does.
add_library(bytetrack STATIC
    src/tracker/bytetrack/byte_tracker.cpp
    src/tracker/bytetrack/kalman_filter.cpp
    src/tracker/bytetrack/lapjv.cpp
    src/tracker/bytetrack/rect.cpp
    src/tracker/bytetrack/strack.cpp
)
target_include_directories(bytetrack PUBLIC
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/include/tracker
)
target_link_libraries(bytetrack PUBLIC Eigen3::Eigen)

# The pipeline itself, one directory per stage.
add_library(vision STATIC
    src/camera/camera.cpp
    src/inference/engine.cpp
    src/inference/detector.cpp
    src/tracker/tracker.cpp
    src/utils/logger.cpp
    src/utils/visualization.cpp
)
target_include_directories(vision PUBLIC
    ${CMAKE_SOURCE_DIR}/include
    ${OpenCV_INCLUDE_DIRS}
    ${TENSORRT_INCLUDE_DIR}
    ${CUDAToolkit_INCLUDE_DIRS}
)
target_link_libraries(vision PUBLIC
    ${OpenCV_LIBS}
    ${TENSORRT_LIBRARY}
    CUDA::cudart
    bytetrack
)

add_executable(tracker src/main.cpp)
target_link_libraries(tracker PRIVATE vision)
set_target_properties(tracker PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

enable_testing()
add_subdirectory(tests)

install(TARGETS tracker RUNTIME DESTINATION bin)
