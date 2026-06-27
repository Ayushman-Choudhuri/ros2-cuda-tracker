#include "tracker.hpp"

#include <map>

SORTTracker::SORTTracker(int max_age, int min_hits, float iou_threshold)
    : max_age_(max_age), min_hits_(min_hits), iou_threshold_(iou_threshold) {}

static float ComputeIoU(const cv::Rect& rect_a, const cv::Rect& rect_b) {
    cv::Rect intersection = rect_a & rect_b;
    float intersection_area = static_cast<float>(intersection.area());
    if (intersection_area <= 0.0F) {
        return 0.0F;
    }
    float union_area = static_cast<float>(rect_a.area() + rect_b.area()) - intersection_area;
    return intersection_area / union_area;
}

std::vector<TrackedDetection> SORTTracker::Update(const std::vector<Detection>& detections) {
    std::vector<cv::Rect> detection_rects;
    detection_rects.reserve(detections.size());
    for (const Detection& detection : detections) {
        detection_rects.push_back(detection.bbox);
    }

    tracker_.Run(detection_rects);
    std::map<int, Track> tracks = tracker_.GetTracks();

    std::vector<TrackedDetection> tracked_detections;

    for (const auto& [track_id, track] : tracks) {
        bool is_active = (track.coast_cycles_ == 0);
        bool is_confirmed_active = is_active && (track.hit_streak_ >= min_hits_);
        bool is_coasting = (track.coast_cycles_ > 0 && track.coast_cycles_ <= max_age_);

        if (!is_confirmed_active && !is_coasting) {
            continue;
        }

        cv::Rect tracked_bbox = track.GetStateAsBbox();

        float max_iou = 0.0F;
        int matched_det_idx = -1;
        for (int det_idx = 0; det_idx < static_cast<int>(detections.size()); ++det_idx) {
            float iou = ComputeIoU(tracked_bbox, detections[det_idx].bbox);
            if (iou > max_iou) {
                max_iou = iou;
                matched_det_idx = det_idx;
            }
        }

        TrackedDetection tracked_detection;
        tracked_detection.track_id = track_id;

        if (matched_det_idx >= 0) {
            tracked_detection.bbox = detections[matched_det_idx].bbox;
            tracked_detection.confidence = detections[matched_det_idx].confidence;
            tracked_detection.class_id = detections[matched_det_idx].class_id;
        } else {
            tracked_detection.bbox = tracked_bbox;
            tracked_detection.confidence = 0.0F;
            tracked_detection.class_id = -1;
        }

        tracked_detections.push_back(tracked_detection);
    }

    return tracked_detections;
}
