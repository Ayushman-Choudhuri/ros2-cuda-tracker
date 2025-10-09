#include "camera.hpp"

#include <iostream>
#include <stdexcept>

Camera::Camera(int deviceId) : deviceId_(deviceId), initialized_(false) {
    if (!initialize()) {
        throw std::runtime_error("Failed to initialize camera with deviceId " +
                                 std::to_string(deviceId));
    }
}

Camera::~Camera() {
    if (frameCapture_.isOpened()) {
        frameCapture_.release();
    }
    cv::destroyAllWindows();
}

bool Camera::initialize() {
    frameCapture_.open(deviceId_);

    if (!frameCapture_.isOpened()) {
        std::cerr << "Error: Could not open camera with deviceID: " << deviceId_ << std::endl;

        return false;
    }

    std::cout << "Camera initialized successfully!" << std::endl;
    int frameWidth = 640;
    int frameHeight = 480;
    int frameFPSSamples = 4;

    frameCapture_.set(cv::CAP_PROP_FRAME_WIDTH, frameWidth);
    frameCapture_.set(cv::CAP_PROP_FRAME_HEIGHT, frameHeight);

    std::cout << "Frame Width: " << frameCapture_.get(cv::CAP_PROP_FRAME_WIDTH) << std::endl;
    std::cout << "Frame Height: " << frameCapture_.get(cv::CAP_PROP_FRAME_HEIGHT) << std::endl;

    cv::Mat tempFrame;
    auto start = cv::getTickCount();
    for (int frameIndex = 0; frameIndex < frameFPSSamples; frameIndex++) {
        frameCapture_ >> tempFrame;
    }
    auto end = cv::getTickCount();
    double timeElapsed = (end - start) / cv::getTickFrequency();
    double estimatedFPS = frameFPSSamples / timeElapsed;

    std::cout << "FPS: " << estimatedFPS << std::endl;

    lastFrameTick_ = cv::getTickCount();
    currentFPS_ = estimatedFPS;

    initialized_ = true;
    return true;
}

void Camera::visualize(const std::string& windowName) {
    if (!initialized_) {
        std::cerr << "Error: Camera not initialized" << std::endl;
        return;
    }

    cv::namedWindow(windowName, cv::WINDOW_AUTOSIZE);

    std::cout << "Starting webcam feed. Press 'q' or 'ESC' to quit." << std::endl;

    cv::Mat frame;

    while (true) {
        frame = getFrame();

        if (frame.empty()) {
            std::cerr << "Error: Could not grab frame." << std::endl;
            break;
        }

        showFrame(frame);

        cv::imshow(windowName, frame);

        int key = cv::waitKey(1);

        if (key == 'q' || key == 'Q' || key == 27) {
            std::cout << "Exiting..." << std::endl;
            break;
        }

        if (key == 's' || key == 'S') {
            std::string filename = "captured_frame_" + std::to_string(cv::getTickCount()) + ".jpg";
            cv::imwrite(filename, frame);
            std::cout << "Frame saved as: " << filename << std::endl;
        }
    }

    cv::destroyWindow(windowName);
}

bool Camera::isOpened() const { return frameCapture_.isOpened(); }

bool Camera::setFrameWidth(int width) {
    if (frameCapture_.isOpened()) {
        frameCapture_.set(cv::CAP_PROP_FRAME_WIDTH, width);
        return true;
    }
    return false;
}

bool Camera::setFrameHeight(int height) {
    if (frameCapture_.isOpened()) {
        frameCapture_.set(cv::CAP_PROP_FRAME_HEIGHT, height);
        return true;
    }
    return false;
}

void Camera::showFrame(cv::Mat& frame) {
    std::string timestamp = "FPS: " + std::to_string(static_cast<float>(currentFPS_));
    cv::putText(frame, timestamp, cv::Point(10, 30), cv::FONT_HERSHEY_SIMPLEX, 0.7,
                cv::Scalar(0, 255, 0), 2);
}

cv::Mat Camera::getFrame() {
    cv::Mat frame;
    frameCapture_ >> frame;

    int64 currentTick = cv::getTickCount();
    double timeDelta = (currentTick - lastFrameTick_) / cv::getTickFrequency();
    double instantFPS = 1.0 / timeDelta;

    currentFPS_ = alphaFPS_ * instantFPS + (1.0 - alphaFPS_) * currentFPS_;

    lastFrameTick_ = currentTick;
    return frame;
}