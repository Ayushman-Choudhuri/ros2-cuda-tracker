#pragma once

#include <opencv2/core.hpp>
#include <opencv2/videoio.hpp>
#include <opencv2/highgui.hpp>
#include <optional>
#include <string>

namespace Defaults {
    inline constexpr int FrameDefaultWidth  = 1280;
    inline constexpr int FrameDefaultHeight = 720;
}

class IInputSource {
    public:
        virtual ~IInputSource() = default;
        virtual std::optional<cv::Mat> GetNextFrame() = 0;
        virtual double GetFps() const = 0;
};

class VideoCaptureBase : public IInputSource {
    public:
        double GetFps() const override { return current_fps_; }
        bool IsOpened() const { return frame_capture_.isOpened(); }
        bool SetFrameWidth(int width) {
            return frame_capture_.isOpened() && frame_capture_.set(cv::CAP_PROP_FRAME_WIDTH, width);
        }
        bool SetFrameHeight(int height) {
            return frame_capture_.isOpened() && frame_capture_.set(cv::CAP_PROP_FRAME_HEIGHT, height);
        }

    protected:
        cv::VideoCapture frame_capture_;

        void UpdateFps() {
            int64_t current_tick = cv::getTickCount();
            double time_delta = static_cast<double>(current_tick - last_frame_tick_) / cv::getTickFrequency();
            if (time_delta > 0.0) {
                double instant_fps = 1.0 / time_delta;
                current_fps_ = kAlphaFps * instant_fps + (1.0 - kAlphaFps) * current_fps_;
            }
            last_frame_tick_ = current_tick;
        }

    private:
        static constexpr double kAlphaFps = 0.1;
        double current_fps_{0.0};
        int64_t last_frame_tick_{cv::getTickCount()};
};

class WebcamCamera : public VideoCaptureBase {
    public:
        WebcamCamera(int deviceID_ = 0, int apiID_ = cv::CAP_ANY);
        WebcamCamera(const WebcamCamera&) = delete;
        WebcamCamera& operator=(const WebcamCamera&) = delete;
        WebcamCamera(WebcamCamera&&) = default;
        ~WebcamCamera() override = default;

        std::optional<cv::Mat> GetNextFrame() override;

    private:
        int deviceID_;
        int apiID_;
};

class VideoFile : public VideoCaptureBase {
    public:
        VideoFile(const std::string& source_file, int apiID_ = cv::CAP_ANY);
        VideoFile(const VideoFile&) = delete;
        VideoFile& operator=(const VideoFile&) = delete;
        VideoFile(VideoFile&&) = default;
        ~VideoFile() override = default;

        std::optional<cv::Mat> GetNextFrame() override;

    private:
        std::string source_file;
        int apiID_;
};
