#include "camera.hpp"
#include <iostream>

int main(int argc, char** argv) {
    std::cout << "=== OpenCV Webcam Capture ===" << std::endl;
    
    // Default camera device ID is 0
    int deviceId = 0;
    
    // Allow user to specify camera device ID via command line
    if (argc > 1) {
        deviceId = std::atoi(argv[1]);
        std::cout << "Using camera device ID: " << deviceId << std::endl;
    }
    
    // Create webcam capture object
    Camera webcam(deviceId);
    
    // Initialize the camera
    if (!webcam.initialize()) {
        std::cerr << "Failed to initialize camera!" << std::endl;
        return -1;
    }
    
    // Optional: Set custom resolution
    // webcam.setFrameWidth(1280);
    // webcam.setFrameHeight(720);
    
    // Start capturing and displaying
    webcam.run("My Webcam Feed");
    
    std::cout << "Application terminated successfully." << std::endl;
    return 0;
}