#include <iostream>
#include <opencv2/core/utils/logger.hpp>
#include <opencv2/highgui.hpp>

#include "camera.hpp"
#include "detector.hpp"
#include "tracker.hpp"
#include "utils.hpp"

constexpr int kDefaultDeviceId = 4;
const std::string kEnginePath = "models/engine/yolov10x_fp16.engine";
constexpr float kConfThreshold = 0.1F;
constexpr int kModelInputSize = 640;
constexpr int kPersonClassId = 0;  // COCO class 0

int main() {
    cv::utils::logging::setLogLevel(cv::utils::logging::LOG_LEVEL_SILENT);

    std::cout << "Camera device : " << kDefaultDeviceId << '\n';
    std::cout << "Engine        : " << kEnginePath << '\n';

    Camera webcam(kDefaultDeviceId);

    Detector detector(kEnginePath, kConfThreshold, kModelInputSize, kModelInputSize, kPersonClassId);

    if (!detector.IsInitialized()) {
        std::cerr << "Detector failed to initialize.\n";
        return 1;
    }

    SORTTracker tracker;

    std::cout << "Running. Press 'q' or ESC to quit.\n";
    cv::namedWindow("Person Tracking", cv::WINDOW_AUTOSIZE);

    while (true) {
        cv::Mat frame = webcam.GetFrame();
        if (frame.empty()) {
            std::cerr << "Empty frame — stopping.\n";
            break;
        }

        std::vector<Detection> detections = detector.Infer(frame);
        std::vector<TrackedDetection> tracks = tracker.Update(detections);
        DrawTrackedDetections(frame, tracks);
        DrawFps(frame, webcam.GetFps());

        cv::imshow("Person Tracking", frame);
        int key = cv::waitKey(1);
        if (key == 'q' || key == 'Q' || key == 27) {
            break;
        }
    }

    cv::destroyAllWindows();
    return 0;
}
