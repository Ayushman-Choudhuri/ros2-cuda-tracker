#pragma once

#include <memory>
#include <opencv2/core.hpp>
#include <string>
#include <vector>

#include "engine.hpp"

struct Detection {
    cv::Rect bbox;
    float confidence;
    int class_id;
};

class Detector {
   public:
        explicit Detector(const std::string& engine_path, float conf_threshold = 0.5f,
                      int input_width = 640, int input_height = 640,
                      int target_class_id = -1);
        ~Detector() = default;

        std::vector<Detection> Infer(const cv::Mat& image);
        bool IsInitialized() const;

   private:
        void PreProcessImage(const cv::Mat& image, float& scale, int& pad_x, int& pad_y);
        std::vector<Detection> PostProcessDetections(const float* output, int num_detections,
                                                 int num_fields, float scale, int pad_x, int pad_y);

        std::unique_ptr<Engine> engine_;
        int input_width_;
        int input_height_;
        float conf_threshold_;
        int target_class_id_;  // -1 = accept all classes
};
