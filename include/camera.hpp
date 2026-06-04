#pragma once

#include <cstdint>
#include <opencv2/opencv.hpp>
#include <string>

class Camera {
   public:
    explicit Camera(int device_id = 0);
    ~Camera();

    bool IsOpened() const;
    bool SetFrameWidth(int width);
    bool SetFrameHeight(int height);
    double GetFps() const;
    cv::Mat GetFrame();

   private:
    static constexpr double kAlphaFps = 0.1;

    cv::VideoCapture camera_handle_;
    int device_id_;
    double current_fps_ = 0.0;
    int64_t last_frame_tick_ = 0;

    bool Initialize();
    void AnnotateFrame(cv::Mat* frame);
    double CalculateFPS();
};
