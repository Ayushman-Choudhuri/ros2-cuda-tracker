#pragma once

#include <cstdint>
#include <memory>
#include <opencv2/core.hpp>
#include <opencv2/videoio.hpp>

namespace vision {

    // V4L2 capture source with an exponentially smoothed frame rate estimate.
    // The capture backend is held behind a pointer so it can be swapped for a
    // vendor SDK later without changing this interface.
    class Camera {
       public:
        explicit Camera(int device_id, int frame_width = 640, int frame_height = 480);

        // Returns an empty frame when the device stops delivering images.
        cv::Mat GetFrame();

        [[nodiscard]] double GetFps() const { return current_fps_; }

       private:
        // Weight of the newest sample in the frame rate estimate. Low enough that a
        // single slow frame does not visibly move the displayed number.
        static constexpr double kFpsSmoothing = 0.1;

        // The first frames after opening a device arrive late while exposure and
        // buffers settle, and would otherwise anchor the estimate far too low.
        static constexpr int kWarmupFrameCount = 4;

        void Warmup();
        void UpdateFps();

        std::unique_ptr<cv::VideoCapture> camera_handle_;
        double current_fps_ = 0.0;
        int64_t last_frame_tick_ = 0;
    };

}  // namespace vision
