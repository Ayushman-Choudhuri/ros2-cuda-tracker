#pragma once

#include <memory>
#include <opencv2/core.hpp>
#include <opencv2/videoio.hpp>

namespace vision {

    class Camera {
       public:
        Camera(int device_id, int frame_width, int frame_height);

        cv::Mat GetFrame();

       private:
        static constexpr int kWarmupFrameCount = 4;

        void Warmup();

        std::unique_ptr<cv::VideoCapture> camera_handle_;
    };

}  // namespace vision
