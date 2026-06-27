cmake_minimum_required(VERSION 3.17)
project(Camera VERSION 1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(OpenCV REQUIRED)
find_package(CUDAToolkit REQUIRED)

# TensorRT — devcontainer (nvcr.io/nvidia/tensorrt:23.03-py3) installs under
# /usr/include/x86_64-linux-gnu and /usr/lib/x86_64-linux-gnu.
find_path(TENSORRT_INCLUDE_DIR NvInfer.h
    PATHS /usr/include/x86_64-linux-gnu /usr/local/include /usr/include)
find_library(TENSORRT_LIBRARY nvinfer
    PATHS /usr/lib/x86_64-linux-gnu /usr/local/lib /usr/lib)

if(NOT TENSORRT_INCLUDE_DIR OR NOT TENSORRT_LIBRARY)
    message(FATAL_ERROR
        "TensorRT not found. Run inside the devcontainer or set "
        "TENSORRT_INCLUDE_DIR and TENSORRT_LIBRARY manually.")
endif()

message(STATUS "TensorRT include : ${TENSORRT_INCLUDE_DIR}")
message(STATUS "TensorRT library : ${TENSORRT_LIBRARY}")
message(STATUS "OpenCV version   : ${OpenCV_VERSION}")

include(FetchContent)
FetchContent_Declare(
    sort_cpp
    GIT_REPOSITORY https://github.com/yasenh/sort-cpp.git
    GIT_TAG        1dcafba
)
# Populate only — do NOT process sort-cpp's CMakeLists.txt.
# sort-cpp uses ${CMAKE_PROJECT_NAME} for its add_executable(), which collides
# with our "Camera" target, and uses plain target_link_libraries() which
# conflicts with our keyword-style usage.
FetchContent_GetProperties(sort_cpp)
if(NOT sort_cpp_POPULATED)
    FetchContent_Populate(sort_cpp)
endif()

file(GLOB SORT_SOURCES "${sort_cpp_SOURCE_DIR}/src/*.cpp")
list(FILTER SORT_SOURCES EXCLUDE REGEX ".*main\\.cpp$")
add_library(sort_lib STATIC ${SORT_SOURCES})
target_include_directories(sort_lib PUBLIC "${sort_cpp_SOURCE_DIR}/include")
target_link_libraries(sort_lib PUBLIC ${OpenCV_LIBS})

set(SOURCES
    src/main.cpp
    src/camera.cpp
    src/engine.cpp
    src/detector.cpp
    src/utils.cpp
    src/tracker.cpp
)

set(HEADERS
    include/camera.hpp
    include/engine.hpp
    include/detector.hpp
    include/utils.hpp
    include/tracker.hpp
)

add_executable(${PROJECT_NAME} ${SOURCES} ${HEADERS})

target_include_directories(${PROJECT_NAME} PRIVATE
    ${OpenCV_INCLUDE_DIRS}
    ${TENSORRT_INCLUDE_DIR}
    ${CUDAToolkit_INCLUDE_DIRS}
    ${CMAKE_SOURCE_DIR}/include
    ${sort_cpp_SOURCE_DIR}/include
)

target_link_libraries(${PROJECT_NAME} PRIVATE
    ${OpenCV_LIBS}
    ${TENSORRT_LIBRARY}
    CUDA::cudart
    sort_lib
)

set_target_properties(${PROJECT_NAME} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

install(TARGETS ${PROJECT_NAME}
    RUNTIME DESTINATION bin
)
