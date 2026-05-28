#include <iostream>
#include <memory>
#include <opencv2/core/utils/logger.hpp>
#include <opencv2/highgui.hpp>
#include <opencv2/imgproc.hpp>

#include "camera.hpp"

constexpr int kDefaultDeviceId = 0;

int main(int argc, char* argv[]) {
    cv::utils::logging::setLogLevel(cv::utils::logging::LOG_LEVEL_SILENT);

    std::cout << "Using camera device ID: " << kDefaultDeviceId << '\n';

    std::unique_ptr<IInputSource> source;

    try {
        if (argc < 2) {
            source = std::make_unique<WebcamCamera>(kDefaultDeviceId);
        } else if (argc == 2) {
            source = std::make_unique<VideoFile>(argv[1]);
        } else {
            std::cout << "Wrong number of arguments passed!\n";
            std::cout << "Use <program name> <source_video_file> OR simply <program name> for webcam...\n";
            return -1;
        }

        cv::namedWindow("Camera feed", cv::WINDOW_NORMAL);
        std::cout << "Starting camera feed. Press 'q' or 'ESC' to quit.\n";

        while (true) {
            auto frame = source->getNextFrame();
            if (!frame) {
                break;
            }

            cv::putText(*frame, "FPS: " + std::to_string(static_cast<int>(source->getFps())),
                        cv::Point(10, 30), cv::FONT_HERSHEY_SIMPLEX, 0.7, cv::Scalar(0, 255, 0), 2);

            cv::imshow("Camera feed", *frame);

            const int key = cv::waitKey(1);
            if (key == 'q' || key == 'Q' || key == 27) {
                break;
            }
        }
    } catch (const std::exception& error) {
        std::cout << "Runtime error: " << error.what() << '\n';
        return -1;
    }

    return 0;
}
