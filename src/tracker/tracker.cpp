#include <opencv2/imgproc.hpp>

#include "tracker/object_tracker.hpp"

namespace vision {
    namespace {

        byte_track::Rect ToByteTrackRect(const cv::Rect& bbox) {
            return {static_cast<float>(bbox.x),
                    static_cast<float>(bbox.y),
                    static_cast<float>(bbox.width),
                    static_cast<float>(bbox.height)};
        }

        cv::Rect ToCvRect(const byte_track::Rect& rect) {
            return {cvRound(rect.Left()),
                    cvRound(rect.Top()),
                    cvRound(rect.Width()),
                    cvRound(rect.Height())};
        }

    }

    std::vector<TrackedDetection> ObjectTracker::Update(const std::vector<Detection>& detections) {
        std::vector<byte_track::Object> objects;
        objects.reserve(detections.size());
        for (const Detection& detection : detections) {
            objects.push_back(
                {ToByteTrackRect(detection.bbox), detection.class_id, detection.confidence});
        }

        std::vector<TrackedDetection> tracked_detections;
        for (const auto& track : tracker_.Update(objects)) {
            tracked_detections.push_back({ToCvRect(track->GetRect()),
                                          track->GetScore(),
                                          track->GetLabel(),
                                          static_cast<int>(track->GetTrackId())});
        }
        return tracked_detections;
    }

}