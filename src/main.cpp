#include <iostream>
#include <opencv2/core/utils/logger.hpp>
#include <opencv2/highgui.hpp>

#include "camera.hpp"
#include "detector.hpp"
#include "utils.hpp"

constexpr int kDefaultDeviceId = 0;
const std::string kEnginePath = "models/yolov10l_fp16.engine";
constexpr float kConfThreshold = 0.1f;
constexpr int kModelInputSize = 640;

int main() {
    cv::utils::logging::setLogLevel(cv::utils::logging::LOG_LEVEL_SILENT);

    std::cout << "Camera device : " << kDefaultDeviceId << '\n';
    std::cout << "Engine        : " << kEnginePath << '\n';

    Camera webcam(kDefaultDeviceId);

    Detector detector(kEnginePath, kConfThreshold, kModelInputSize, kModelInputSize);

    if (!detector.IsInitialized()) {
        std::cerr << "Detector failed to initialize.\n";
        return 1;
    }

    std::cout << "Running. Press 'q' or ESC to quit.\n";
    cv::namedWindow("YOLOv10 Detection", cv::WINDOW_AUTOSIZE);

    while (true) {
        cv::Mat frame = webcam.GetFrame();
        if (frame.empty()) {
            std::cerr << "Empty frame — stopping.\n";
            break;
        }

        std::vector<Detection> detections = detector.Infer(frame);
        DrawDetections(frame, detections);
        DrawFps(frame, webcam.GetFps());

        cv::imshow("YOLOv10 Detection", frame);
        int key = cv::waitKey(1);
        if (key == 'q' || key == 'Q' || key == 27)
            break;
    }

    cv::destroyAllWindows();
    return 0;
}
