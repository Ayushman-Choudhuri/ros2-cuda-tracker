#pragma once

#include <opencv2/core.hpp>
#include <string>
#include <vector>

#include "inference/engine.hpp"
#include "utils/detection_types.hpp"

namespace vision {

    inline constexpr int kAllClasses = -1;

    struct DetectorConfig {
        float confidence_threshold = 0.1F;
        int input_size = 0;
        int target_class_id = kAllClasses;
    };

    class Detector {
       public:
        Detector(const std::string& engine_path, const DetectorConfig& config);

        std::vector<Detection> Detect(const cv::Mat& image);

       private:
        struct Letterbox {
            float scale = 1.0F;
            cv::Size scaled_size;
            int pad_x = 0;
            int pad_y = 0;
        };

        void ValidateEngineLayout() const;

        [[nodiscard]] Letterbox ComputeLetterbox(const cv::Size& image_size) const;
        [[nodiscard]] cv::Mat MakeInputBlob(const cv::Mat& image, const Letterbox& letterbox) const;
        Letterbox UploadImage(const cv::Mat& image);
        [[nodiscard]] std::vector<float> DownloadOutput() const;
        [[nodiscard]] std::vector<Detection> DecodeDetections(const std::vector<float>& output,
                                                              const Letterbox& letterbox) const;

        TensorRtEngine engine_;
        DetectorConfig config_;
    };

}  // namespace vision
