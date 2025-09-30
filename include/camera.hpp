#ifndef WEBCAM_CAPTURE_HPP
#define WEBCAM_CAPTURE_HPP

#include <opencv2/opencv.hpp>
#include <string>

class Camera{

public: 

    explicit Camera(int deviceId = 0);

    ~Camera();

    bool initialize();

    void run(const std::string& windowName = "Camera Feed");

    bool isOpened() const;

    bool setFrameWidth(int width);

    bool setFrameHeight(int height);

    double getFPS() const;

private: 
    cv::VideoCapture frameCapture_;
    int deviceId_;
    cv::Mat frame_;
    bool initialized_;

    void processFrame(cv::Mat& frame);
};

#endif