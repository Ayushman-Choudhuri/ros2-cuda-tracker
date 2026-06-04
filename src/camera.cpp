#include "camera.hpp"

#include <cstdint>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>

static constexpr int kDefaultWidth = 640;
static constexpr int kDefaultHeight = 480;
static constexpr int kInitWarmupFrameCount = 4;
static constexpr double kDefaultFps = 30.0;

Camera::Camera(int device_id) : device_id_(device_id) {
    if (!Initialize()) {
        throw std::runtime_error("Failed to initialize camera with deviceId " +
                                 std::to_string(device_id));
    }
}

Camera::~Camera() {
    if (camera_handle_.isOpened()) {
        camera_handle_.release();
    }
}

bool Camera::Initialize() {
    camera_handle_.open(device_id_);

    if (!camera_handle_.isOpened()) {
        std::cerr << "Error: Could not open camera with deviceID: " << device_id_ << '\n';
        return false;
    }

    std::cout << "Camera initialized successfully!\n";

    camera_handle_.set(cv::CAP_PROP_FRAME_WIDTH, kDefaultWidth);
    camera_handle_.set(cv::CAP_PROP_FRAME_HEIGHT, kDefaultHeight);

    std::cout << "Frame Width: " << camera_handle_.get(cv::CAP_PROP_FRAME_WIDTH) << '\n';
    std::cout << "Frame Height: " << camera_handle_.get(cv::CAP_PROP_FRAME_HEIGHT) << '\n';

    cv::Mat temp_frame;
    int64_t start = cv::getTickCount();
    for (int sample_idx = 0; sample_idx < kInitWarmupFrameCount; ++sample_idx) {
        camera_handle_ >> temp_frame;
    }
    int64_t end = cv::getTickCount();
    double time_elapsed = (end - start) / cv::getTickFrequency();

    if (time_elapsed > 0.0) {
        current_fps_ = kInitWarmupFrameCount / time_elapsed;
        std::cout << "FPS: " << current_fps_ << '\n';
    } else {
        current_fps_ = kDefaultFps;
    }

    last_frame_tick_ = cv::getTickCount();
    return true;
}

bool Camera::IsOpened() const { return camera_handle_.isOpened(); }

bool Camera::SetFrameWidth(int width) {
    return camera_handle_.isOpened() && camera_handle_.set(cv::CAP_PROP_FRAME_WIDTH, width);
}

bool Camera::SetFrameHeight(int height) {
    return camera_handle_.isOpened() && camera_handle_.set(cv::CAP_PROP_FRAME_HEIGHT, height);
}

double Camera::GetFps() const { return current_fps_; }

void Camera::AnnotateFrame(cv::Mat* frame) {
    std::ostringstream oss;
    oss << "FPS: " << std::fixed << std::setprecision(1) << current_fps_;
    cv::putText(*frame, oss.str(), cv::Point(10, 30), cv::FONT_HERSHEY_SIMPLEX, 0.7,
                cv::Scalar(0, 255, 0), 2);
}

double Camera::CalculateFPS() {
    int64_t current_tick = cv::getTickCount();
    double time_delta = (current_tick - last_frame_tick_) / cv::getTickFrequency();

    if (time_delta > 0.0) {
        double instant_fps = 1.0 / time_delta;
        current_fps_ = kAlphaFps * instant_fps + (1 - kAlphaFps) * current_fps_;
    }

    last_frame_tick_ = current_tick;
    return current_fps_;
}

cv::Mat Camera::GetFrame() {
    cv::Mat frame;
    camera_handle_ >> frame;
    CalculateFPS();
    return frame;
}
