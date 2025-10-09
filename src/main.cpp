#include <iostream>
#include <opencv2/core/utils/logger.hpp>

#include "camera.hpp"

#define DEFAULT_DEVICE_ID 4

int main(int argc, char** argv) {
    cv::utils::logging::setLogLevel(cv::utils::logging::LOG_LEVEL_SILENT);

    std::cout << "Using camera device ID: " << DEFAULT_DEVICE_ID << std::endl;

    Camera webcam(DEFAULT_DEVICE_ID);

    // cv::Mat frame = webcam.getFrame();

    webcam.visualize("Camera feed");
    // std::cout << "Application terminated successfully." << std::endl;
    return 0;
}