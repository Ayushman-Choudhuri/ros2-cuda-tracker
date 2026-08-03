#include "camera/camera.hpp"

#include <stdexcept>
#include <string>

#include "utils/logger.hpp"

namespace vision {

    Camera::Camera(int device_id, int frame_width, int frame_height)
        : camera_handle_(std::make_unique<cv::VideoCapture>(device_id)) {
        if (!camera_handle_->isOpened()) {
            throw std::runtime_error("Failed to open camera device " + std::to_string(device_id));
        }

        camera_handle_->set(cv::CAP_PROP_FRAME_WIDTH, frame_width);
        camera_handle_->set(cv::CAP_PROP_FRAME_HEIGHT, frame_height);
        Warmup();

        LOG_INFO("Camera") << "device " << device_id << " | "
                           << camera_handle_->get(cv::CAP_PROP_FRAME_WIDTH) << "x"
                           << camera_handle_->get(cv::CAP_PROP_FRAME_HEIGHT);
    }

    cv::Mat Camera::GetFrame() {
        cv::Mat frame;
        *camera_handle_ >> frame;
        return frame;
    }

    // The first frames off a V4L2 device arrive before auto-exposure and auto-white-balance
    // have settled.
    void Camera::Warmup() {
        cv::Mat discarded_frame;
        for (int frame_index = 0; frame_index < kWarmupFrameCount; ++frame_index) {
            *camera_handle_ >> discarded_frame;
        }
    }

}  // namespace vision
