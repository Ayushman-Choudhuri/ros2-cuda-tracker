cmake_minimum_required(VERSION 3.10)
project(Camera VERSION 1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(OpenCV REQUIRED)

include_directories(
    ${OpenCV_INCLUDE_DIRS}
    ${CMAKE_SOURCE_DIR}/include
)

set(SOURCES
    src/main.cpp
    src/camera.cpp
)

set(HEADERS
    include/camera.hpp
)

add_executable(${PROJECT_NAME} ${SOURCES} ${HEADERS})

target_link_libraries(${PROJECT_NAME} ${OpenCV_LIBS})

set_target_properties(${PROJECT_NAME} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

message(STATUS "OpenCV version: ${OpenCV_VERSION}")
message(STATUS "OpenCV libraries: ${OpenCV_LIBS}")

install(TARGETS ${PROJECT_NAME}
    RUNTIME DESTINATION bin
)