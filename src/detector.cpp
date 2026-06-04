#include "detector.hpp"

#include <cuda_fp16.h>

#include <algorithm>
#include <opencv2/dnn.hpp>
#include <opencv2/imgproc.hpp>
#include <stdexcept>
#include <vector>

Detector::Detector(const std::string& engine_path, float conf_threshold, int input_width,
                   int input_height)
    : engine_(std::make_unique<Engine>(engine_path)),
      input_width_(input_width),
      input_height_(input_height),
      conf_threshold_(conf_threshold) {
}

bool Detector::IsInitialized() const {
    return engine_ && engine_->IsLoaded();
}

void Detector::PreProcessImage(const cv::Mat& image, float& scale, int& pad_x, int& pad_y) {
    scale = std::min(static_cast<float>(input_width_) / image.cols,
                     static_cast<float>(input_height_) / image.rows);

    int scaled_w = static_cast<int>(std::round(image.cols * scale));
    int scaled_h = static_cast<int>(std::round(image.rows * scale));
    pad_x = (input_width_ - scaled_w) / 2;
    pad_y = (input_height_ - scaled_h) / 2;

    cv::Mat resized;
    cv::resize(image, resized, cv::Size(scaled_w, scaled_h), 0, 0, cv::INTER_LINEAR);

    cv::Mat letterboxed(input_height_, input_width_, CV_8UC3, cv::Scalar(0, 0, 0));
    resized.copyTo(letterboxed(cv::Rect(pad_x, pad_y, scaled_w, scaled_h)));

    cv::Mat blob = cv::dnn::blobFromImage(letterboxed, 1.0 / 255.0,
                                          cv::Size(input_width_, input_height_),
                                          cv::Scalar(0, 0, 0), /*swapRB=*/true,
                                          /*crop=*/false, CV_32F);

    cudaMemcpy(engine_->GetInputBuffer(), blob.ptr<float>(), blob.total() * sizeof(float),
               cudaMemcpyHostToDevice);
}

std::vector<Detection> Detector::PostProcessDetections(const float* output, int num_detections,
                                                       int num_fields, float scale, int pad_x,
                                                       int pad_y) {
    std::vector<Detection> detections;
    detections.reserve(num_detections);

    for (int det_idx = 0; det_idx < num_detections; ++det_idx) {
        const float* det_fields = output + det_idx * num_fields;
        float confidence = det_fields[4];
        if (confidence < conf_threshold_)
            continue;

        // Undo letterbox: subtract padding then invert uniform scale.
        int bbox_left   = static_cast<int>(std::max(0.0F, (det_fields[0] - pad_x) / scale));
        int bbox_top    = static_cast<int>(std::max(0.0F, (det_fields[1] - pad_y) / scale));
        int bbox_right  = static_cast<int>(std::max(0.0F, (det_fields[2] - pad_x) / scale));
        int bbox_bottom = static_cast<int>(std::max(0.0F, (det_fields[3] - pad_y) / scale));

        detections.push_back({cv::Rect(bbox_left, bbox_top, bbox_right - bbox_left,
                                       bbox_bottom - bbox_top),
                               confidence, static_cast<int>(det_fields[5])});
    }

    return detections;
}

std::vector<Detection> Detector::Infer(const cv::Mat& image) {
    if (!IsInitialized())
        throw std::runtime_error("Detector not initialized");

    float scale;
    int pad_x, pad_y;
    PreProcessImage(image, scale, pad_x, pad_y);

    engine_->Infer();

    int num_det = engine_->GetOutputNumDetections();
    int num_fields = engine_->GetOutputNumFields();
    std::vector<float> output(static_cast<size_t>(num_det) * num_fields);

    if (engine_->GetOutputDataType() == nvinfer1::DataType::kHALF) {
        std::vector<__half> half_buf(output.size());
        cudaMemcpyAsync(half_buf.data(), engine_->GetOutputBuffer(),
                        half_buf.size() * sizeof(__half), cudaMemcpyDeviceToHost,
                        engine_->GetStream());
        cudaStreamSynchronize(engine_->GetStream());
        for (size_t field_idx = 0; field_idx < output.size(); ++field_idx)
            output[field_idx] = __half2float(half_buf[field_idx]);
    } else {
        cudaMemcpyAsync(output.data(), engine_->GetOutputBuffer(), output.size() * sizeof(float),
                        cudaMemcpyDeviceToHost, engine_->GetStream());
        cudaStreamSynchronize(engine_->GetStream());
    }

    return PostProcessDetections(output.data(), num_det, num_fields, scale, pad_x, pad_y);
}
