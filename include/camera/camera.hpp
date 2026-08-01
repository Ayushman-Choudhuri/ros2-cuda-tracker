#pragma once

#include <cstdint>
#include <memory>
#include <opencv2/core.hpp>
#include <opencv2/videoio.hpp>

namespace vision {

    class Camera {
       public:
        explicit Camera(int device_id, int frame_width = 640, int frame_height = 480);

        cv::Mat GetFrame();

        [[nodiscard]] double GetFps() const { return current_fps_; }

       private:

        static constexpr double kFpsSmoothing = 0.1;

        static constexpr int kWarmupFrameCount = 4;

        void Warmup();
        void UpdateFps();

        std::unique_ptr<cv::VideoCapture> camera_handle_;
        double current_fps_ = 0.0;
        int64_t last_frame_tick_ = 0;
    };

}
