#include <iostream>
#include <memory>
#include <opencv2/core/utils/logger.hpp>
#include <opencv2/highgui.hpp>
#include <opencv2/imgproc.hpp>

#include "camera.hpp"
#include "detector.hpp"
#include "utils.hpp"

constexpr int kDefaultDeviceId = 4;
const std::string kEnginePath = "models/engine/yolov10n_fp16.engine";
constexpr float kConfThreshold = 0.1f;
constexpr int kModelInputSize = 640;

int main(int argc, char* argv[]) {
    cv::utils::logging::setLogLevel(cv::utils::logging::LOG_LEVEL_SILENT);

    std::cout << "Camera device : " << kDefaultDeviceId << '\n';
    std::cout << "Engine        : " << kEnginePath << '\n';

    std::unique_ptr<IInputSource> source;

    try {
        if (argc < 2) {
            source = std::make_unique<WebcamCamera>(kDefaultDeviceId);
        } else if (argc == 2) {
            source = std::make_unique<VideoFile>(argv[1]);
        } else {
            std::cerr << "Wrong number of arguments passed!\n";
            std::cerr << "Use <program name> <source_video_file> OR simply <program name> for webcam...\n";
            return 1;
        }

        Detector detector(kEnginePath, kConfThreshold, kModelInputSize, kModelInputSize);

        if (!detector.IsInitialized()) {
            std::cerr << "Detector failed to initialize.\n";
            return 1;
        }

        cv::namedWindow("YOLOv10 Detection", cv::WINDOW_NORMAL);
        std::cout << "Running. Press 'q' or ESC to quit.\n";

        while (true) {
            auto frame = source->GetNextFrame();
            if (!frame) {
                break;
            }

            std::vector<Detection> detections = detector.Infer(*frame);
            DrawDetections(*frame, detections);
            DrawFps(*frame, source->GetFps());

            cv::imshow("YOLOv10 Detection", *frame);

            const int key = cv::waitKey(1);
            if (key == 'q' || key == 'Q' || key == 27) {
                break;
            }
        }
    } catch (const std::exception& error) {
        cv::destroyAllWindows();
        std::cerr << "Runtime error: " << error.what() << '\n';
        return 1;
    }

    cv::destroyAllWindows();
    return 0;
}
