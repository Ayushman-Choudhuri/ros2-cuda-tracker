#include "camera.hpp"
#include <opencv2/core/utils/logger.hpp>
#include <iostream>

#define DEFAULT_DEVICE_ID 0

int main(int argc, char** argv) {
    cv::utils::logging::setLogLevel(cv::utils::logging::LOG_LEVEL_SILENT);
        
    
    std::cout << "Using camera device ID: " << DEFAULT_DEVICE_ID << std::endl;
    
    
    Camera webcam(DEFAULT_DEVICE_ID);
    
    if (!webcam.initialize()) {
        std::cerr << "Failed to initialize camera!" << std::endl;
        return -1;
    }
    
    webcam.run("My Webcam Feed");
    
    std::cout << "Application terminated successfully." << std::endl;
    return 0;
}