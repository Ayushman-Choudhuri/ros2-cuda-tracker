#include "inference/detector.hpp"

#include <cuda_fp16.h>

#include <algorithm>
#include <cmath>
#include <opencv2/dnn.hpp>
#include <opencv2/imgproc.hpp>
#include <stdexcept>
#include <string>

namespace vision {
    namespace {

        enum DetectionField {
            kFieldLeft = 0,
            kFieldTop = 1,
            kFieldRight = 2,
            kFieldBottom = 3,
            kFieldConfidence = 4,
            kFieldClassId = 5,
            kFieldCount = 6,
        };

        constexpr double kPixelToUnitScale = 1.0 / 255.0;

        // The network was trained on raw normalized pixels, so no mean is subtracted;
        // the same value doubles as the letterbox border fill.
        const cv::Scalar kZeroPixel(0, 0, 0);

    }  // namespace

    Detector::Detector(const std::string& engine_path, const DetectorConfig& config)
        : engine_(engine_path), config_(config) {
        ValidateEngineLayout();
    }

    void Detector::ValidateEngineLayout() const {
        // A letterbox smaller than the engine's input is a short cudaMemcpy into a
        // larger buffer: it succeeds, then inference runs on rows at the wrong stride
        // and silently returns nothing.
        const InputLayout& input_layout = engine_.GetInputLayout();
        if (input_layout.width != config_.input_size || input_layout.height != config_.input_size) {
            throw std::runtime_error(
                "model_input_size (" + std::to_string(config_.input_size) +
                ") does not match the engine's input tensor (" +
                std::to_string(input_layout.width) + "x" + std::to_string(input_layout.height) +
                "). Set model_input_size to " + std::to_string(input_layout.width) +
                ", or rebuild the engine at " + std::to_string(config_.input_size) + ".");
        }

        // DecodeDetections indexes every field up to kFieldClassId, so a narrower row
        // would read past the end of the downloaded tensor.
        const OutputLayout& output_layout = engine_.GetOutputLayout();
        if (output_layout.field_count < kFieldCount) {
            throw std::runtime_error(
                "Engine output carries " + std::to_string(output_layout.field_count) +
                " fields per detection, but at least " +
                std::to_string(static_cast<int>(kFieldCount)) +
                " (left, top, right, bottom, confidence, class id) are required");
        }
    }

    std::vector<Detection> Detector::Detect(const cv::Mat& image) {
        const Letterbox letterbox = UploadImage(image);
        engine_.Infer();
        return DecodeDetections(DownloadOutput(), letterbox);
    }

    Detector::Letterbox Detector::ComputeLetterbox(const cv::Size& image_size) const {
        const auto input_size = static_cast<float>(config_.input_size);

        Letterbox letterbox;
        letterbox.scale = std::min(input_size / image_size.width, input_size / image_size.height);
        letterbox.scaled_size = {static_cast<int>(std::round(image_size.width * letterbox.scale)),
                                 static_cast<int>(std::round(image_size.height * letterbox.scale))};

        letterbox.pad_x = (config_.input_size - letterbox.scaled_size.width) / 2;
        letterbox.pad_y = (config_.input_size - letterbox.scaled_size.height) / 2;
        return letterbox;
    }

    cv::Mat Detector::MakeInputBlob(const cv::Mat& image, const Letterbox& letterbox) const {
        cv::Mat resized;
        cv::resize(image, resized, letterbox.scaled_size, 0, 0, cv::INTER_LINEAR);

        cv::Mat padded(config_.input_size, config_.input_size, CV_8UC3, kZeroPixel);
        resized.copyTo(
            padded(cv::Rect(cv::Point(letterbox.pad_x, letterbox.pad_y), letterbox.scaled_size)));

        return cv::dnn::blobFromImage(padded,
                                      kPixelToUnitScale,
                                      padded.size(),
                                      kZeroPixel,
                                      /*swapRB=*/true,
                                      /*crop=*/false,
                                      CV_32F);
    }

    Detector::Letterbox Detector::UploadImage(const cv::Mat& image) {
        const Letterbox letterbox = ComputeLetterbox(image.size());
        const cv::Mat blob = MakeInputBlob(image, letterbox);

        ThrowOnCudaError(cudaMemcpy(engine_.GetInputBuffer(),
                                    blob.ptr<float>(),
                                    blob.total() * sizeof(float),
                                    cudaMemcpyHostToDevice),
                         "cudaMemcpy of input blob");
        return letterbox;
    }

    std::vector<float> Detector::DownloadOutput() const {
        const OutputLayout& layout = engine_.GetOutputLayout();
        const size_t value_count = static_cast<size_t>(layout.detection_count) * layout.field_count;
        const bool is_half = layout.data_type == nvinfer1::DataType::kHALF;

        // An FP16 engine lands in the staging buffer and is widened afterwards; an FP32
        // one is copied straight into the result, leaving the staging buffer empty.
        std::vector<float> output(value_count);
        std::vector<__half> half_output(is_half ? value_count : 0);

        void* host_destination = is_half ? static_cast<void*>(half_output.data()) : output.data();
        const size_t byte_count = value_count * (is_half ? sizeof(__half) : sizeof(float));

        ThrowOnCudaError(cudaMemcpyAsync(host_destination,
                                         engine_.GetOutputBuffer(),
                                         byte_count,
                                         cudaMemcpyDeviceToHost,
                                         engine_.GetStream()),
                         "cudaMemcpyAsync of output tensor");
        ThrowOnCudaError(cudaStreamSynchronize(engine_.GetStream()), "cudaStreamSynchronize");

        if (is_half) {
            std::transform(
                half_output.begin(), half_output.end(), output.begin(), [](__half value) {
                    return __half2float(value);
                });
        }
        return output;
    }

    std::vector<Detection> Detector::DecodeDetections(const std::vector<float>& output,
                                                      const Letterbox& letterbox) const {
        const OutputLayout& layout = engine_.GetOutputLayout();

        std::vector<Detection> detections;
        detections.reserve(static_cast<size_t>(layout.detection_count));

        const auto to_image_x = [&letterbox](float value) {
            return static_cast<int>(std::max(0.0F, (value - letterbox.pad_x) / letterbox.scale));
        };
        const auto to_image_y = [&letterbox](float value) {
            return static_cast<int>(std::max(0.0F, (value - letterbox.pad_y) / letterbox.scale));
        };

        for (int detection_index = 0; detection_index < layout.detection_count; ++detection_index) {
            const float* fields = output.data() + detection_index * layout.field_count;

            const float confidence = fields[kFieldConfidence];
            if (confidence < config_.confidence_threshold) {
                continue;
            }

            const int class_id = static_cast<int>(fields[kFieldClassId]);
            if (config_.target_class_id != kAllClasses && class_id != config_.target_class_id) {
                continue;
            }

            const int left = to_image_x(fields[kFieldLeft]);
            const int top = to_image_y(fields[kFieldTop]);
            const int right = to_image_x(fields[kFieldRight]);
            const int bottom = to_image_y(fields[kFieldBottom]);

            detections.push_back(
                {cv::Rect(left, top, right - left, bottom - top), confidence, class_id});
        }
        return detections;
    }

}  // namespace vision
