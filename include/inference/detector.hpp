#pragma once

#include <opencv2/core.hpp>
#include <string>
#include <vector>

#include "inference/tensorrt_engine.hpp"

namespace vision {

    struct Detection {
        cv::Rect bbox;
        float confidence = 0.0F;
        int class_id = -1;
    };

    inline constexpr int kAllClasses = -1;

    struct DetectorConfig {
        std::string engine_path;
        float confidence_threshold = 0.25F;
        int input_width = 640;
        int input_height = 640;
        int target_class_id = kAllClasses;
    };

    // Runs a YOLOv10 engine end to end: letterbox on the CPU, inference on the GPU,
    // then decode the NMS-free head back into image coordinates.
    class Detector {
       public:
        explicit Detector(const DetectorConfig& config);

        std::vector<Detection> Detect(const cv::Mat& image);

       private:
        // Maps image coordinates onto the square network input: the image is scaled
        // to fit and centred, so undoing it means removing the padding then the scale.
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

}  // namespace vision
