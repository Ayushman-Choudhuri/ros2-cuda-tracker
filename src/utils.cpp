#include "utils.hpp"

#include <algorithm>
#include <iomanip>
#include <opencv2/imgproc.hpp>
#include <sstream>
#include <unordered_map>

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
// Cached: hue is a pure function of class_id so the cv::Mat alloc runs once per unique id.
cv::Scalar ClassColor(int class_id) {
    static std::unordered_map<int, cv::Scalar> cache;
    auto cache_iter = cache.find(class_id);
    if (cache_iter != cache.end()) {
        return cache_iter->second;
    }
    int hue = (class_id * 137) % 180;
    cv::Mat hsv(1, 1, CV_8UC3, cv::Scalar(hue, 210, 220));
    cv::Mat bgr;
    cv::cvtColor(hsv, bgr, cv::COLOR_HSV2BGR);
    const auto* bgr_data = bgr.ptr<uint8_t>(0);
    cv::Scalar color(bgr_data[0], bgr_data[1], bgr_data[2]);
    cache[class_id] = color;
    return color;
}

// Renders a filled-background text label above bbox.
void DrawLabel(cv::Mat& frame, const cv::Rect& bbox, const std::string& label,
               const cv::Scalar& color) {
    constexpr int kFont = cv::FONT_HERSHEY_SIMPLEX;
    constexpr double kScale = 0.5;
    constexpr int kThick = 1;
    int baseline = 0;
    cv::Size text_size = cv::getTextSize(label, kFont, kScale, kThick, &baseline);

    int text_y = std::max(bbox.y - 4, text_size.height + 2);
    cv::Point label_top_left(bbox.x, text_y - text_size.height - 2);
    cv::Point label_bottom_right(bbox.x + text_size.width, text_y + baseline);

    cv::rectangle(frame, label_top_left, label_bottom_right, color, cv::FILLED);
    cv::putText(frame, label, cv::Point(bbox.x, text_y), kFont, kScale,
                cv::Scalar(255, 255, 255), kThick, cv::LINE_AA);
}

}  // namespace

void DrawDetections(cv::Mat& frame, const std::vector<Detection>& detections,
                    const std::vector<std::string>& class_names) {
    const auto& names = class_names.empty() ? kCocoNames : class_names;

    for (const auto& detection : detections) {
        cv::Rect bbox = detection.bbox & cv::Rect(0, 0, frame.cols, frame.rows);
        if (bbox.area() <= 0) {
            continue;
        }

        const cv::Scalar color = ClassColor(detection.class_id);
        cv::rectangle(frame, bbox, color, 2);

        std::string label_name =
            (detection.class_id >= 0 && detection.class_id < static_cast<int>(names.size()))
                ? names[detection.class_id]
                : "cls" + std::to_string(detection.class_id);
        std::ostringstream label_stream;
        label_stream << label_name << ' ' << std::fixed << std::setprecision(2)
                     << detection.confidence;

        DrawLabel(frame, bbox, label_stream.str(), color);
    }
}

void DrawFps(cv::Mat& frame, double fps) {
    std::ostringstream fps_stream;
    fps_stream << "FPS: " << std::fixed << std::setprecision(1) << fps;
    cv::putText(frame, fps_stream.str(), cv::Point(10, 30), cv::FONT_HERSHEY_SIMPLEX, 0.7,
                cv::Scalar(0, 255, 0), 2, cv::LINE_AA);
}

void DrawTrackedDetections(cv::Mat& frame, const std::vector<TrackedDetection>& tracks,
                           const std::vector<std::string>& class_names) {
    const auto& names = class_names.empty() ? kCocoNames : class_names;

    for (const auto& track : tracks) {
        cv::Rect bbox = track.bbox & cv::Rect(0, 0, frame.cols, frame.rows);
        if (bbox.area() <= 0) {
            continue;
        }

        const cv::Scalar color =
            (track.class_id >= 0) ? ClassColor(track.class_id) : ClassColor(track.track_id);
        cv::rectangle(frame, bbox, color, 2);

        std::string label;
        if (track.class_id >= 0) {
            std::string class_name =
                (track.class_id < static_cast<int>(names.size()))
                    ? names[track.class_id]
                    : "cls" + std::to_string(track.class_id);
            std::ostringstream label_stream;
            label_stream << class_name << " #" << track.track_id << ' '
                         << std::fixed << std::setprecision(2) << track.confidence;
            label = label_stream.str();
        } else {
            label = "#" + std::to_string(track.track_id);
        }

        DrawLabel(frame, bbox, label, color);
    }
}
