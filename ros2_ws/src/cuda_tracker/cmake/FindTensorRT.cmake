# TensorRT ships no CMake config package. Discovery keys off
# CMAKE_LIBRARY_ARCHITECTURE rather than a hardcoded x86_64-linux-gnu, so the
# package also configures on the aarch64 Jetson deploy target.

find_path(TensorRT_INCLUDE_DIR
    NAMES NvInfer.h
    HINTS ${TensorRT_ROOT} ENV TensorRT_ROOT
    PATH_SUFFIXES include
    PATHS
        /usr/include/${CMAKE_LIBRARY_ARCHITECTURE}
        /usr/include
        /usr/local/include
)

find_library(TensorRT_LIBRARY
    NAMES nvinfer
    HINTS ${TensorRT_ROOT} ENV TensorRT_ROOT
    PATH_SUFFIXES lib lib64
    PATHS
        /usr/lib/${CMAKE_LIBRARY_ARCHITECTURE}
        /usr/lib
        /usr/local/lib
)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(TensorRT
    REQUIRED_VARS TensorRT_INCLUDE_DIR TensorRT_LIBRARY
)

if(TensorRT_FOUND AND NOT TARGET TensorRT::TensorRT)
    add_library(TensorRT::TensorRT UNKNOWN IMPORTED)
    set_target_properties(TensorRT::TensorRT PROPERTIES
        IMPORTED_LOCATION "${TensorRT_LIBRARY}"
        INTERFACE_INCLUDE_DIRECTORIES "${TensorRT_INCLUDE_DIR}"
    )
endif()

mark_as_advanced(TensorRT_INCLUDE_DIR TensorRT_LIBRARY)
