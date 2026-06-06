#include "camera.hpp"

#include <iostream>
#include <optional>
#include <string>
#include <opencv2/core.hpp>
#include <opencv2/highgui.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/videoio.hpp>


WebcamCamera::WebcamCamera(int deviceID_, int apiID_) : deviceID_{deviceID_}, apiID_{apiID_} {
    frame_capture_.open(deviceID_, apiID_);
    if (!frame_capture_.isOpened()) {
        throw std::runtime_error("Error: Could not open camera with deviceID: " + std::to_string(deviceID_));
    }
    std::cout << "Camera initialized successfully!\n";
    frame_capture_.set(cv::CAP_PROP_FRAME_WIDTH, Defaults::FrameDefaultWidth);
    frame_capture_.set(cv::CAP_PROP_FRAME_HEIGHT, Defaults::FrameDefaultHeight);

    std::cout << "Frame Width: " << frame_capture_.get(cv::CAP_PROP_FRAME_WIDTH) << '\n';
    std::cout << "Frame Height: " << frame_capture_.get(cv::CAP_PROP_FRAME_HEIGHT) << '\n';
}

auto WebcamCamera::GetNextFrame() -> std::optional<cv::Mat> {
    cv::Mat frame;
    bool read_img_ok = frame_capture_.read(frame);
    if (!read_img_ok) {
        throw std::runtime_error("frame_capture_.read() failed, device maybe disconnected");
    }
    if (frame.empty()) {
        throw std::runtime_error("frame_capture_.read() succeeded but returned an empty frame. The camera may not be delivering images.");
    }

    UpdateFps();
    return frame;
}

VideoFile::VideoFile(const std::string& source_file, int apiID_) : source_file{source_file}, apiID_{apiID_} {
    frame_capture_.open(source_file, apiID_);
    if (!frame_capture_.isOpened()) {
        throw std::runtime_error("ERROR: Unable to open source file '" + source_file + "'. Please check the file path and format.");
    }
    frame_capture_.set(cv::CAP_PROP_FRAME_WIDTH, Defaults::FrameDefaultWidth);
    frame_capture_.set(cv::CAP_PROP_FRAME_HEIGHT, Defaults::FrameDefaultHeight);

    std::cout << "Frame Width: " << frame_capture_.get(cv::CAP_PROP_FRAME_WIDTH) << '\n';
    std::cout << "Frame Height: " << frame_capture_.get(cv::CAP_PROP_FRAME_HEIGHT) << '\n';
}

auto VideoFile::GetNextFrame() -> std::optional<cv::Mat> {
    cv::Mat frame;
    bool read_file_ok = frame_capture_.read(frame);

    if (!read_file_ok) {
        return std::nullopt;
    }
    if (frame.empty()) {
        throw std::runtime_error("Frame read was successful but the frame is empty. The video file may be corrupted or at EOF.");
    }

    UpdateFps();
    return frame;
}
