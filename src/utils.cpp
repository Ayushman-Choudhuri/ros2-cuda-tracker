#include "utils.hpp"

#include <algorithm>
#include <iomanip>
#include <opencv2/imgproc.hpp>
#include <sstream>

namespace {

// COCO-80 class names in index order.
const std::vector<std::string> kCocoNames = {"person",        "bicycle",      "car",
                                             "motorcycle",    "airplane",     "bus",
                                             "train",         "truck",        "boat",
                                             "traffic light", "fire hydrant", "stop sign",
                                             "parking meter", "bench",        "bird",
                                             "cat",           "dog",          "horse",
                                             "sheep",         "cow",          "elephant",
                                             "bear",          "zebra",        "giraffe",
                                             "backpack",      "umbrella",     "handbag",
                                             "tie",           "suitcase",     "frisbee",
                                             "skis",          "snowboard",    "sports ball",
                                             "kite",          "baseball bat", "baseball glove",
                                             "skateboard",    "surfboard",    "tennis racket",
                                             "bottle",        "wine glass",   "cup",
                                             "fork",          "knife",        "spoon",
                                             "bowl",          "banana",       "apple",
                                             "sandwich",      "orange",       "broccoli",
                                             "carrot",        "hot dog",      "pizza",
                                             "donut",         "cake",         "chair",
                                             "couch",         "potted plant", "bed",
                                             "dining table",  "toilet",       "tv",
                                             "laptop",        "mouse",        "remote",
                                             "keyboard",      "cell phone",   "microwave",
                                             "oven",          "toaster",      "sink",
                                             "refrigerator",  "book",         "clock",
                                             "vase",          "scissors",     "teddy bear",
                                             "hair drier",    "toothbrush"};

// Deterministic per-class color via golden-angle hue spacing in HSV.
cv::Scalar ClassColor(int class_id) {
    int hue = (class_id * 137) % 180;
    cv::Mat hsv(1, 1, CV_8UC3, cv::Scalar(hue, 210, 220));
    cv::Mat bgr;
    cv::cvtColor(hsv, bgr, cv::COLOR_HSV2BGR);
    const auto* p = bgr.ptr<uint8_t>(0);
    return cv::Scalar(p[0], p[1], p[2]);
}

}  // namespace

void DrawDetections(cv::Mat& frame, const std::vector<Detection>& detections,
                    const std::vector<std::string>& class_names) {
    const auto& names = class_names.empty() ? kCocoNames : class_names;

    for (const auto& det : detections) {
        const cv::Scalar color = ClassColor(det.class_id);

        // Clamp bbox to frame bounds.
        cv::Rect bbox = det.bbox & cv::Rect(0, 0, frame.cols, frame.rows);
        if (bbox.area() <= 0)
            continue;

        cv::rectangle(frame, bbox, color, 2);

        // Build label string: "person 0.87"
        std::string label_name =
            (det.class_id >= 0 && det.class_id < static_cast<int>(names.size()))
                ? names[det.class_id]
                : "cls" + std::to_string(det.class_id);
        std::ostringstream oss;
        oss << label_name << ' ' << std::fixed << std::setprecision(2) << det.confidence;
        const std::string label = oss.str();

        constexpr int kFont = cv::FONT_HERSHEY_SIMPLEX;
        constexpr double kScale = 0.5;
        constexpr int kThick = 1;
        int baseline = 0;
        cv::Size text_sz = cv::getTextSize(label, kFont, kScale, kThick, &baseline);

        // Place label above box; clamp so it stays inside frame.
        int text_y = std::max(bbox.y - 4, text_sz.height + 2);
        cv::Point tl(bbox.x, text_y - text_sz.height - 2);
        cv::Point br(bbox.x + text_sz.width, text_y + baseline);

        // Filled background for readability.
        cv::rectangle(frame, tl, br, color, cv::FILLED);
        cv::putText(frame, label, cv::Point(bbox.x, text_y), kFont, kScale,
                    cv::Scalar(255, 255, 255), kThick, cv::LINE_AA);
    }
}

void DrawFps(cv::Mat& frame, double fps) {
    std::ostringstream oss;
    oss << "FPS: " << std::fixed << std::setprecision(1) << fps;
    cv::putText(frame, oss.str(), cv::Point(10, 30), cv::FONT_HERSHEY_SIMPLEX, 0.7,
                cv::Scalar(0, 255, 0), 2, cv::LINE_AA);
}
