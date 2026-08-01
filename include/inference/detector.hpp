#pragma once

#include <opencv2/core.hpp>
#include <string>
#include <vector>

#include "inference/engine.hpp"

namespace vision {

    struct Detection {
        cv::Rect bbox;
        float confidence = 0.0F;
        int class_id = -1;
    };

    class Detector {
       public:
        static constexpr int kAllClasses = -1;

        Detector(const std::string& engine_path,
                 float confidence_threshold,
                 int input_width,
                 int input_height,
                 int target_class_id = kAllClasses);

        std::vector<Detection> Detect(const cv::Mat& image);

       private:
        struct Letterbox {
            float scale = 1.0F;
            cv::Size scaled_size;
            int pad_x = 0;
            int pad_y = 0;
        };

        [[nodiscard]] Letterbox ComputeLetterbox(const cv::Size& image_size) const;
        [[nodiscard]] cv::Mat MakeInputBlob(const cv::Mat& image, const Letterbox& letterbox) const;
        Letterbox UploadImage(const cv::Mat& image);
        [[nodiscard]] std::vector<float> DownloadOutput() const;
        [[nodiscard]] std::vector<Detection> DecodeDetections(const std::vector<float>& output,
                                                              const Letterbox& letterbox) const;

        TensorRtEngine engine_;
        int input_width_;
        int input_height_;
        float confidence_threshold_;
        int target_class_id_;
    };

} 
