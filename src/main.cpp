#include <iostream>
#include <opencv2/core/utils/logger.hpp>

#include "camera.hpp"

constexpr int kDefaultDeviceId = 4;

int main() {
    cv::utils::logging::setLogLevel(cv::utils::logging::LOG_LEVEL_SILENT);

    std::cout << "Using camera device ID: " << kDefaultDeviceId << '\n';

    Camera webcam(kDefaultDeviceId);
    webcam.Visualize("Camera feed");
    return 0;
}
