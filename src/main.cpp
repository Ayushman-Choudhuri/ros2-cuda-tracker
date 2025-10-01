#include "camera.hpp"
#include <opencv2/core/utils/logger.hpp>
#include <iostream>

int main(int argc, char** argv) {
    cv::utils::logging::setLogLevel(cv::utils::logging::LOG_LEVEL_SILENT);

    std::cout << "=== OpenCV Webcam Capture ===" << std::endl;
    
    int deviceId = 0;
    
    if (argc > 1) {
        deviceId = std::atoi(argv[1]);
        std::cout << "Using camera device ID: " << deviceId << std::endl;
    }
    
    Camera webcam(deviceId);
    
    if (!webcam.initialize()) {
        std::cerr << "Failed to initialize camera!" << std::endl;
        return -1;
    }
    
    webcam.run("My Webcam Feed");
    
    std::cout << "Application terminated successfully." << std::endl;
    return 0;
}