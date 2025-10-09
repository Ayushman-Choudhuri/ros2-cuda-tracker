#ifndef WEBCAM_CAPTURE_HPP
#define WEBCAM_CAPTURE_HPP

#include <opencv2/opencv.hpp>
#include <string>

class Camera {
   public:
    explicit Camera(int deviceId = 0);

    ~Camera();

    bool initialize();

    void visualize(const std::string& windowName = "Camera Feed");

    bool isOpened() const;

    bool setFrameWidth(int width);

    bool setFrameHeight(int height);

    double getFPS() const;

    cv::Mat getFrame();

   private:
    cv::VideoCapture frameCapture_;
    int deviceId_;
    bool initialized_;
    double currentFPS_;
    int64 lastFrameTick_;
    const double alphaFPS_ = 0.1;
    void showFrame(cv::Mat& frame);
};

#endif