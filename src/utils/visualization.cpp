#include "utils/visualization.hpp"

#include <algorithm>
#include <iomanip>
#include <opencv2/imgproc.hpp>
#include <sstream>

namespace vision {
    namespace {

        const std::vector<std::string> kCocoNames = {
            "person",         "bicycle",    "car",           "motorcycle",    "airplane",
            "bus",            "train",      "truck",         "boat",          "traffic light",
            "fire hydrant",   "stop sign",  "parking meter", "bench",         "bird",
            "cat",            "dog",        "horse",         "sheep",         "cow",
            "elephant",       "bear",       "zebra",         "giraffe",       "backpack",
            "umbrella",       "handbag",    "tie",           "suitcase",      "frisbee",
            "skis",           "snowboard",  "sports ball",   "kite",          "baseball bat",
            "baseball glove", "skateboard", "surfboard",     "tennis racket", "bottle",
            "wine glass",     "cup",        "fork",          "knife",         "spoon",
            "bowl",           "banana",     "apple",         "sandwich",      "orange",
            "broccoli",       "carrot",     "hot dog",       "pizza",         "donut",
            "cake",           "chair",      "couch",         "potted plant",  "bed",
            "dining table",   "toilet",     "tv",            "laptop",        "mouse",
            "remote",         "keyboard",   "cell phone",    "microwave",     "oven",
            "toaster",        "sink",       "refrigerator",  "book",          "clock",
            "vase",           "scissors",   "teddy bear",    "hair drier",    "toothbrush",
        };


        constexpr int kFontFace = cv::FONT_HERSHEY_SIMPLEX;
        constexpr int kBoxThickness = 2;

        constexpr double kLabelFontScale = 0.5;
        constexpr int kLabelFontThickness = 1;
        constexpr int kLabelConfidenceDigits = 2;

        constexpr int kLabelPadding = 2;
        constexpr int kLabelGapAboveBox = 4;

        constexpr double kFpsFontScale = 0.7;
        constexpr int kFpsDigits = 1;
        const cv::Point kFpsOrigin(10, 30);

        const cv::Scalar kLabelTextColor(255, 255, 255);
        const cv::Scalar kFpsColor(0, 255, 0);

        constexpr int kHueStep = 137;
        constexpr int kHueRange = 180;
        constexpr int kBoxSaturation = 210;
        constexpr int kBoxValue = 220;

        cv::Scalar ClassColor(int class_id) {
            const cv::Mat hsv(
                1,
                1,
                CV_8UC3,
                cv::Scalar((class_id * kHueStep) % kHueRange, kBoxSaturation, kBoxValue));
            cv::Mat bgr;
            cv::cvtColor(hsv, bgr, cv::COLOR_HSV2BGR);

            return bgr.at<cv::Vec3b>(0, 0);
        }

        std::string ClassName(int class_id) {
            if (class_id >= 0 && class_id < static_cast<int>(kCocoNames.size())) {
                return kCocoNames[class_id];
            }
            return "class " + std::to_string(class_id);
        }

        std::string TrackLabel(const TrackedDetection& track) {
            if (track.class_id < 0) {
                return "id " + std::to_string(track.track_id);
            }

            std::ostringstream label;
            label << ClassName(track.class_id) << " id " << track.track_id << " " << std::fixed
                  << std::setprecision(kLabelConfidenceDigits) << track.confidence;
            return label.str();
        }

        void DrawLabel(cv::Mat& frame,
                       const cv::Rect& bbox,
                       const std::string& label,
                       const cv::Scalar& color) {
            int baseline = 0;
            const cv::Size text_size =
                cv::getTextSize(label, kFontFace, kLabelFontScale, kLabelFontThickness, &baseline);

            const int text_bottom =
                std::max(bbox.y - kLabelGapAboveBox, text_size.height + kLabelPadding);
            const cv::Point background_top_left(bbox.x,
                                                text_bottom - text_size.height - kLabelPadding);
            const cv::Point background_bottom_right(bbox.x + text_size.width,
                                                    text_bottom + baseline);

            cv::rectangle(frame, background_top_left, background_bottom_right, color, cv::FILLED);
            cv::putText(frame,
                        label,
                        cv::Point(bbox.x, text_bottom),
                        kFontFace,
                        kLabelFontScale,
                        kLabelTextColor,
                        kLabelFontThickness,
                        cv::LINE_AA);
        }

    }

    void DrawTracks(cv::Mat& frame, const std::vector<TrackedDetection>& tracks) {
        for (const TrackedDetection& track : tracks) {
            const cv::Rect bbox = track.bbox & cv::Rect(0, 0, frame.cols, frame.rows);
            if (bbox.area() <= 0) {
                continue;
            }

            const cv::Scalar color =
                ClassColor(track.class_id >= 0 ? track.class_id : track.track_id);
            cv::rectangle(frame, bbox, color, kBoxThickness);
            DrawLabel(frame, bbox, TrackLabel(track), color);
        }
    }

    void DrawFps(cv::Mat& frame, double fps) {
        std::ostringstream label;
        label << "FPS: " << std::fixed << std::setprecision(kFpsDigits) << fps;

        cv::putText(frame,
                    label.str(),
                    kFpsOrigin,
                    kFontFace,
                    kFpsFontScale,
                    kFpsColor,
                    kBoxThickness,
                    cv::LINE_AA);
    }

} 
