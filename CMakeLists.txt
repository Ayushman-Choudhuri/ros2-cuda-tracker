cmake_minimum_required(VERSION 3.17)
project(Camera VERSION 1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(OpenCV REQUIRED)
find_package(CUDAToolkit REQUIRED)

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

find_package(Eigen3 3.3 QUIET NO_MODULE)
if(NOT Eigen3_FOUND)
    message(STATUS "Eigen3 not found; fetching a header-only copy")
    include(FetchContent)
    FetchContent_Declare(
        eigen
        GIT_REPOSITORY https://gitlab.com/libeigen/eigen.git
        GIT_TAG        3.4.0
    )
    FetchContent_GetProperties(eigen)
    if(NOT eigen_POPULATED)
        FetchContent_Populate(eigen)
    endif()
    add_library(eigen_headers INTERFACE)
    target_include_directories(eigen_headers INTERFACE "${eigen_SOURCE_DIR}")
    add_library(Eigen3::Eigen ALIAS eigen_headers)
endif()

file(GLOB BYTETRACK_SOURCES "${CMAKE_SOURCE_DIR}/src/bytetrack/*.cpp")
add_library(bytetrack_lib STATIC ${BYTETRACK_SOURCES})
target_include_directories(bytetrack_lib PUBLIC "${CMAKE_SOURCE_DIR}/include")
target_link_libraries(bytetrack_lib PUBLIC Eigen3::Eigen)

set(SOURCES
    src/main.cpp
    src/camera.cpp
    src/engine.cpp
    src/detector.cpp
    src/utils.cpp
    src/tracker.cpp
)

add_executable(${PROJECT_NAME} ${SOURCES})

target_include_directories(${PROJECT_NAME} PRIVATE
    ${OpenCV_INCLUDE_DIRS}
    ${TENSORRT_INCLUDE_DIR}
    ${CUDAToolkit_INCLUDE_DIRS}
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(${PROJECT_NAME} PRIVATE
    ${OpenCV_LIBS}
    ${TENSORRT_LIBRARY}
    CUDA::cudart
    bytetrack_lib
)

set_target_properties(${PROJECT_NAME} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

install(TARGETS ${PROJECT_NAME}
    RUNTIME DESTINATION bin
)
