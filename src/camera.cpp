#include "camera.hpp"

#include <cstdint>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>

static constexpr int kDefaultWidth = 640;
static constexpr int kDefaultHeight = 480;
static constexpr int kFpsSampleCount = 4;

Camera::Camera(int device_id) : device_id_(device_id) {
    if (!Initialize()) {
        throw std::runtime_error("Failed to initialize camera with deviceId " +
                                 std::to_string(device_id));
    }
}

Camera::~Camera() {
    if (frame_capture_.isOpened()) {
        frame_capture_.release();
    }
}

bool Camera::Initialize() {
    frame_capture_.open(device_id_);

    if (!frame_capture_.isOpened()) {
        std::cerr << "Error: Could not open camera with deviceID: " << device_id_ << '\n';
        return false;
    }

    std::cout << "Camera initialized successfully!\n";

    frame_capture_.set(cv::CAP_PROP_FRAME_WIDTH, kDefaultWidth);
    frame_capture_.set(cv::CAP_PROP_FRAME_HEIGHT, kDefaultHeight);

    std::cout << "Frame Width: " << frame_capture_.get(cv::CAP_PROP_FRAME_WIDTH) << '\n';
    std::cout << "Frame Height: " << frame_capture_.get(cv::CAP_PROP_FRAME_HEIGHT) << '\n';

    cv::Mat temp_frame;
    int64_t start = cv::getTickCount();
    for (int i = 0; i < kFpsSampleCount; ++i) {
        frame_capture_ >> temp_frame;
    }
    int64_t end = cv::getTickCount();
    double time_elapsed = (end - start) / cv::getTickFrequency();

    if (time_elapsed > 0.0) {
        current_fps_ = kFpsSampleCount / time_elapsed;
        std::cout << "FPS: " << current_fps_ << '\n';
    } else {
        current_fps_ = 30.0;
    }

    last_frame_tick_ = cv::getTickCount();
    return true;
}

void Camera::Visualize(const std::string& window_name) {
    cv::namedWindow(window_name, cv::WINDOW_AUTOSIZE);
    std::cout << "Starting webcam feed. Press 'q' or 'ESC' to quit.\n";

    cv::Mat frame;
    while (true) {
        frame = GetFrame();

        if (frame.empty()) {
            std::cerr << "Error: Could not grab frame.\n";
            break;
        }

        AnnotateFrame(&frame);
        cv::imshow(window_name, frame);

        int key = cv::waitKey(1);
        if (key == 'q' || key == 'Q' || key == 27) {
            std::cout << "Exiting...\n";
            break;
        }

        if (key == 's' || key == 'S') {
            std::string filename =
                "captured_frame_" + std::to_string(cv::getTickCount()) + ".jpg";
            cv::imwrite(filename, frame);
            std::cout << "Frame saved as: " << filename << '\n';
        }
    }

    cv::destroyWindow(window_name);
}

bool Camera::IsOpened() const { return frame_capture_.isOpened(); }

bool Camera::SetFrameWidth(int width) {
    return frame_capture_.isOpened() && frame_capture_.set(cv::CAP_PROP_FRAME_WIDTH, width);
}

bool Camera::SetFrameHeight(int height) {
    return frame_capture_.isOpened() && frame_capture_.set(cv::CAP_PROP_FRAME_HEIGHT, height);
}

double Camera::GetFps() const { return current_fps_; }

void Camera::AnnotateFrame(cv::Mat* frame) {
    std::ostringstream oss;
    oss << "FPS: " << std::fixed << std::setprecision(1) << current_fps_;
    cv::putText(*frame, oss.str(), cv::Point(10, 30), cv::FONT_HERSHEY_SIMPLEX, 0.7,
                cv::Scalar(0, 255, 0), 2);
}

cv::Mat Camera::GetFrame() {
    cv::Mat frame;
    frame_capture_ >> frame;

    int64_t current_tick = cv::getTickCount();
    double time_delta = (current_tick - last_frame_tick_) / cv::getTickFrequency();

    if (time_delta > 0.0) {
        double instant_fps = 1.0 / time_delta;
        current_fps_ = kAlphaFps * instant_fps + (1.0 - kAlphaFps) * current_fps_;
    }

    last_frame_tick_ = current_tick;
    return frame;
}
