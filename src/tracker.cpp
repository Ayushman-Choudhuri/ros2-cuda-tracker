#include "tracker.hpp"

#include <opencv2/imgproc.hpp>

ByteTracker::ByteTracker(int frame_rate, int track_buffer, float track_thresh, float high_thresh,
                         float match_thresh)
    : tracker_(frame_rate, track_buffer, track_thresh, high_thresh, match_thresh) {
}

std::vector<TrackedDetection> ByteTracker::Update(const std::vector<Detection>& detections) {
    std::vector<byte_track::Object> objects;
    objects.reserve(detections.size());
    for (const Detection& detection : detections) {
        byte_track::Rect rect(
            static_cast<float>(detection.bbox.x), static_cast<float>(detection.bbox.y),
            static_cast<float>(detection.bbox.width), static_cast<float>(detection.bbox.height));
        objects.emplace_back(rect, detection.class_id, detection.confidence);
    }

    std::vector<TrackedDetection> tracked_detections;
    for (const auto& track : tracker_.Update(objects)) {
        const byte_track::Rect& rect = track->GetRect();
        TrackedDetection tracked_detection;
        tracked_detection.bbox = cv::Rect(cvRound(rect.Left()), cvRound(rect.Top()),
                                          cvRound(rect.Width()), cvRound(rect.Height()));
        tracked_detection.confidence = track->GetScore();
        tracked_detection.class_id = track->GetLabel();
        tracked_detection.track_id = static_cast<int>(track->GetTrackId());
        tracked_detections.push_back(tracked_detection);
    }
    return tracked_detections;
}
