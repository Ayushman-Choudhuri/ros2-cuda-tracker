#include "camera/camera.hpp"

#include <iostream>
#include <stdexcept>
#include <string>

namespace vision {

    Camera::Camera(int device_id, int frame_width, int frame_height)
        : camera_handle_(std::make_unique<cv::VideoCapture>(device_id)) {
        if (!camera_handle_->isOpened()) {
            throw std::runtime_error("Failed to open camera device " + std::to_string(device_id));
        }

        camera_handle_->set(cv::CAP_PROP_FRAME_WIDTH, frame_width);
        camera_handle_->set(cv::CAP_PROP_FRAME_HEIGHT, frame_height);
        Warmup();

        std::cout << "[Camera] device " << device_id << " | "
                  << camera_handle_->get(cv::CAP_PROP_FRAME_WIDTH) << "x"
                  << camera_handle_->get(cv::CAP_PROP_FRAME_HEIGHT) << " | " << current_fps_
                  << " fps\n";
    }

    cv::Mat Camera::GetFrame() {
        cv::Mat frame;
        *camera_handle_ >> frame;
        UpdateFps();
        return frame;
    }

    void Camera::Warmup() {
        cv::Mat discarded_frame;
        const int64_t start_tick = cv::getTickCount();
        for (int frame_index = 0; frame_index < kWarmupFrameCount; ++frame_index) {
            *camera_handle_ >> discarded_frame;
        }
        const double elapsed_seconds = (cv::getTickCount() - start_tick) / cv::getTickFrequency();

        current_fps_ = elapsed_seconds > 0.0 ? kWarmupFrameCount / elapsed_seconds : 0.0;
        last_frame_tick_ = cv::getTickCount();
    }

    void Camera::UpdateFps() {
        const int64_t current_tick = cv::getTickCount();
        const double elapsed_seconds = (current_tick - last_frame_tick_) / cv::getTickFrequency();

        if (elapsed_seconds > 0.0) {
            const double instant_fps = 1.0 / elapsed_seconds;
            current_fps_ = kFpsSmoothing * instant_fps + (1.0 - kFpsSmoothing) * current_fps_;
        }
        last_frame_tick_ = current_tick;
    }

}
