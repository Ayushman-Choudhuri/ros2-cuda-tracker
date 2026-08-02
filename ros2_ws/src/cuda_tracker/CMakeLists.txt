cmake_minimum_required(VERSION 3.17)
# C as well as CXX: rosidl generates C typesupport libraries for the messages
# below, and they cannot build with only CXX enabled.
project(cuda_tracker VERSION 0.1.0 LANGUAGES C CXX)

if(NOT CMAKE_CXX_STANDARD)
    set(CMAKE_CXX_STANDARD 17)
endif()
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)

find_package(ament_cmake REQUIRED)
find_package(OpenCV REQUIRED)
find_package(CUDAToolkit REQUIRED)
find_package(TensorRT REQUIRED)
find_package(std_msgs REQUIRED)
find_package(rclcpp REQUIRED)
find_package(rclcpp_lifecycle REQUIRED)
find_package(rclcpp_components REQUIRED)
find_package(lifecycle_msgs REQUIRED)
find_package(sensor_msgs REQUIRED)
find_package(cv_bridge REQUIRED)
find_package(rosidl_default_generators REQUIRED)

# The package's own messages. Deliberately minimal compared with
# vision_msgs/Detection2DArray, which nests a full 6-DoF pose and a 36-element
# covariance per detection for a purely 2D pixel-space result.
rosidl_generate_interfaces(${PROJECT_NAME}
    msg/TrackedDetection.msg
    msg/TrackedDetectionArray.msg
    DEPENDENCIES std_msgs
)
# Targets in this same package cannot use ament_target_dependencies to reach the
# interfaces they generate, so link the typesupport target directly.
rosidl_get_typesupport_target(cuda_tracker_typesupport ${PROJECT_NAME} rosidl_typesupport_cpp)

# Header-only. The TensorRT base images used for deployment do not ship Eigen,
# so fall back to a pinned source drop rather than failing to configure.
find_package(Eigen3 3.3 QUIET NO_MODULE)
if(NOT Eigen3_FOUND)
    include(FetchContent)
    FetchContent_Declare(eigen
        GIT_REPOSITORY https://gitlab.com/libeigen/eigen.git
        GIT_TAG 3.4.0)
    FetchContent_GetProperties(eigen)
    if(NOT eigen_POPULATED)
        FetchContent_Populate(eigen)
    endif()
    # Populate-only: adding Eigen's own CMake project would flip BUILD_TESTING
    # and take colcon's test targets down with it.
    add_library(eigen_headers INTERFACE)
    target_include_directories(eigen_headers INTERFACE ${eigen_SOURCE_DIR})
    add_library(Eigen3::Eigen ALIAS eigen_headers)
endif()

message(STATUS "TensorRT include : ${TensorRT_INCLUDE_DIR}")
message(STATUS "TensorRT library : ${TensorRT_LIBRARY}")
message(STATUS "OpenCV version   : ${OpenCV_VERSION}")

# Vendored ByteTrack-cpp port (MIT, see src/tracker/bytetrack/LICENSE). Its own
# target so upstream code compiles under its own flags, untouched by ours.
# include/tracker is on the include path so the vendored sources keep spelling
# their headers the way upstream does.
add_library(bytetrack STATIC
    src/tracker/bytetrack/byte_tracker.cpp
    src/tracker/bytetrack/kalman_filter.cpp
    src/tracker/bytetrack/lapjv.cpp
    src/tracker/bytetrack/rect.cpp
    src/tracker/bytetrack/strack.cpp
)
target_include_directories(bytetrack PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/include/tracker
)
target_link_libraries(bytetrack PUBLIC Eigen3::Eigen)
set_property(TARGET bytetrack PROPERTY POSITION_INDEPENDENT_CODE ON)

add_library(tracker_node SHARED
    src/camera/camera.cpp
    src/inference/engine.cpp
    src/inference/detector.cpp
    src/tracker/tracker.cpp
    src/utils/logger.cpp
    src/utils/conversions.cpp
    src/node/tracker_node.cpp
)
target_include_directories(tracker_node PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${OpenCV_INCLUDE_DIRS}
)

# Models and engines live in the repository, never in /opt. A node's cwd is
# arbitrary under `ros2 run`, `ros2 launch` and systemd, so a bare relative
# engine_path could not be resolved at runtime; baking the repo root in at
# configure time lets one resolve against the other. An absolute engine_path
# still overrides this entirely.
get_filename_component(CUDA_TRACKER_REPO_ROOT
    "${CMAKE_CURRENT_SOURCE_DIR}/../../.." ABSOLUTE)
target_compile_definitions(tracker_node PRIVATE
    CUDA_TRACKER_REPO_ROOT="${CUDA_TRACKER_REPO_ROOT}")
# Plain signature: ament_target_dependencies below uses it too, and CMake
# forbids mixing the plain and keyword forms on one target.
target_link_libraries(tracker_node
    ${OpenCV_LIBS}
    TensorRT::TensorRT
    CUDA::cudart
    bytetrack
    "${cuda_tracker_typesupport}"
)
ament_target_dependencies(tracker_node
    rclcpp
    rclcpp_lifecycle
    rclcpp_components
    lifecycle_msgs
    sensor_msgs
    std_msgs
    cv_bridge
)
rclcpp_components_register_node(tracker_node
    PLUGIN "vision::TrackerNode"
    EXECUTABLE cuda_tracker_node
)

install(TARGETS tracker_node
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION bin
)

install(DIRECTORY launch config
    DESTINATION share/${PROJECT_NAME}
)

# No test suites at present — test/ was removed deliberately. `colcon test` is a
# no-op until suites come back.

ament_package()
