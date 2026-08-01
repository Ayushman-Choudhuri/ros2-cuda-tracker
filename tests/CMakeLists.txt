find_package(GTest REQUIRED)
include(GoogleTest)

# Links only `bytetrack`, so the suite stays runnable on machines without CUDA
# or TensorRT. Keep new GPU-free suites that way.
add_executable(bytetrack_test bytetrack_test.cpp)
target_link_libraries(bytetrack_test PRIVATE bytetrack GTest::gtest_main)
set_target_properties(bytetrack_test PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)
gtest_discover_tests(bytetrack_test)

# Compiles logger.cpp directly rather than linking `vision`, which would drag in
# CUDA and TensorRT for a suite that needs neither.
add_executable(logger_test logger_test.cpp ${CMAKE_SOURCE_DIR}/src/utils/logger.cpp)
target_include_directories(logger_test PRIVATE ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(logger_test PRIVATE GTest::gtest_main)
set_target_properties(logger_test PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)
gtest_discover_tests(logger_test)
