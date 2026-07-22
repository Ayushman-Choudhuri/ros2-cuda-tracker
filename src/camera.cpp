#include "camera.hpp"

#include <cstdint>
#include <iostream>
#include <stdexcept>

static constexpr int kDefaultWidth = 640;
static constexpr int kDefaultHeight = 480;
static constexpr int kInitWarmupFrameCount = 4;

Camera::Camera(int device_id) : device_id_(device_id) {
    if (!Initialize()) {
        throw std::runtime_error("Failed to initialize camera with deviceId " +
                                 std::to_string(device_id));
    }
}

bool Camera::Initialize() {
    camera_handle_ = std::make_unique<cv::VideoCapture>(device_id_);

    if (!camera_handle_->isOpened()) {
        std::cerr << "Error: Could not open camera with deviceID: " << device_id_ << '\n';
        return false;
    }

    std::cout << "Camera initialized successfully!\n";

    camera_handle_->set(cv::CAP_PROP_FRAME_WIDTH, kDefaultWidth);
    camera_handle_->set(cv::CAP_PROP_FRAME_HEIGHT, kDefaultHeight);

    std::cout << "Frame Width: " << camera_handle_->get(cv::CAP_PROP_FRAME_WIDTH) << '\n';
    std::cout << "Frame Height: " << camera_handle_->get(cv::CAP_PROP_FRAME_HEIGHT) << '\n';

    Warmup();
    return true;
}

void Camera::Warmup() {
    cv::Mat temp_frame;
    int64_t start_tick = cv::getTickCount();

    for (int sample_idx = 0; sample_idx < kInitWarmupFrameCount; ++sample_idx) {
        *camera_handle_ >> temp_frame;
    }

    int64_t end_tick = cv::getTickCount();
    double time_elapsed = (end_tick - start_tick) / cv::getTickFrequency();

    current_fps_ = kInitWarmupFrameCount / time_elapsed;
    std::cout << "FPS: " << current_fps_ << '\n';

    last_frame_tick_ = cv::getTickCount();
    std::cout<<"Camera Warmed up"<<std::endl;
}

bool Camera::IsOpened() const {
    return camera_handle_->isOpened();
}

bool Camera::SetFrameWidth(int width) {
    return camera_handle_->isOpened() && camera_handle_->set(cv::CAP_PROP_FRAME_WIDTH, width);
}

bool Camera::SetFrameHeight(int height) {
    return camera_handle_->isOpened() && camera_handle_->set(cv::CAP_PROP_FRAME_HEIGHT, height);
}

double Camera::GetFps() const { 
    return current_fps_; 
}

double Camera::CalculateFps() {
    int64_t current_tick = cv::getTickCount();
    double time_delta = (current_tick - last_frame_tick_) / cv::getTickFrequency();

    if (time_delta > 0.0) {
        double instant_fps = 1.0 / time_delta;
        current_fps_ = kAlphaFps * instant_fps + (1.0 - kAlphaFps) * current_fps_;
    }

    last_frame_tick_ = current_tick;
    return current_fps_;
}

cv::Mat Camera::GetFrame() {
    cv::Mat frame;
    *camera_handle_ >> frame;
    CalculateFps();
    return frame;
}
