#pragma once

#include <cstdint>
#include <memory>
#include <opencv2/opencv.hpp>
#include <string>

class Camera {
   public:
    explicit Camera(int device_id = 0);

    [[nodiscard]] bool IsOpened() const;
    bool SetFrameWidth(int width);
    bool SetFrameHeight(int height);
    [[nodiscard]] double GetFps() const;
    cv::Mat GetFrame();

   private:
    static constexpr double kAlphaFps = 0.1;

    std::unique_ptr<cv::VideoCapture> camera_handle_;
    int device_id_;
    double current_fps_ = 0.0;
    int64_t last_frame_tick_ = 0;

    bool Initialize();
    void Warmup();
    double CalculateFps();
};
