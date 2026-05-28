#include <opencv2/core.hpp>
#include <opencv2/videoio.hpp>
#include <opencv2/highgui.hpp>
#include <iostream>
#include <stdio.h>

namespace Defaults {
    inline constexpr int FrameDefaultWidth  = 1280;
    inline constexpr int FrameDefaultHeight = 720;
}

// Pure interface
class IInputSource {
    public:
        virtual ~IInputSource() = default;
        virtual std::optional<cv::Mat> getNextFrame() = 0;
        virtual double getFps() const = 0;
};

// Abstract base class
class VideoCaptureBase : public IInputSource {
    public:
        double getFps() const override { return current_fps_; }

    protected:
        cv::VideoCapture cap;

        void updateFps() {
            int64_t current_tick = cv::getTickCount();
            double time_delta = static_cast<double>(current_tick - last_frame_tick_) / cv::getTickFrequency();
            if (time_delta > 0.0) {
                current_fps_ = 1.0 / time_delta;
            }
            last_frame_tick_ = current_tick;
        }

private:
    double current_fps_{0.0};
    int64_t last_frame_tick_{cv::getTickCount()};
};

class WebcamCamera : public VideoCaptureBase {
    public:
        WebcamCamera(int deviceID = 0, int apiID = cv::CAP_ANY);
        WebcamCamera(const WebcamCamera&) = delete;  // copy constructor
        WebcamCamera& operator=(const WebcamCamera&) = delete;  // copy assignment constructor
        WebcamCamera(WebcamCamera&&) = default;  // move constructor
        ~WebcamCamera() override = default;      // destructor

        std::optional<cv::Mat> getNextFrame() override;
        
    private:
        int deviceID;
        int apiID;
};

class VideoFile : public VideoCaptureBase {
    public:
        VideoFile(const std::string& source_file, int apiID = cv::CAP_ANY);
        VideoFile(const VideoFile&) = delete;
        VideoFile& operator=(const VideoFile&) = delete;
        VideoFile(VideoFile&&) = default;
        ~VideoFile() override = default;

        std::optional<cv::Mat> getNextFrame() override;
        
    private:
        std::string source_file;
        int apiID;
};
