#include "tracker.hpp"

#include <map>
#include <set>

namespace {

float ComputeIoU(const cv::Rect& rect_a, const cv::Rect& rect_b) {
    cv::Rect intersection = rect_a & rect_b;
    float intersection_area = static_cast<float>(intersection.area());
    if (intersection_area <= 0.0F) {
        return 0.0F;
    }
    float union_area = static_cast<float>(rect_a.area() + rect_b.area()) - intersection_area;
    return intersection_area / union_area;
}

}  // namespace

SORTTracker::SORTTracker(int min_hits) : min_hits_(min_hits) {}

std::vector<TrackedDetection> SORTTracker::Update(const std::vector<Detection>& detections) {
    std::vector<cv::Rect> detection_rects;
    detection_rects.reserve(detections.size());
    for (const Detection& detection : detections) {
        detection_rects.push_back(detection.bbox);
    }

    // Pre-match: mirror Run()'s predict+associate to get the authoritative
    // track_id→detection_index map. Replaces the post-hoc greedy IoU re-match
    // which had no exclusion set and a 0.0F IoU floor.
    std::map<int, int> track_to_det_idx;
    std::set<int> matched_det_indices;

    std::map<int, Track> pre_run_tracks = tracker_.GetTracks();
    std::set<int> pre_run_ids;
    for (const auto& [track_id, track] : pre_run_tracks) {
        pre_run_ids.insert(track_id);
    }

    if (!pre_run_tracks.empty() && !detection_rects.empty()) {
        for (auto& [track_id, track] : pre_run_tracks) {
            track.Predict();
        }

        std::map<int, cv::Rect> matched;
        std::vector<cv::Rect> unmatched_det;
        Tracker::AssociateDetectionsToTrackers(detection_rects, pre_run_tracks,
                                               matched, unmatched_det);

        for (const auto& [track_id, rect] : matched) {
            for (int det_idx = 0; det_idx < static_cast<int>(detection_rects.size()); ++det_idx) {
                if (detection_rects[det_idx] == rect) {
                    track_to_det_idx[track_id] = det_idx;
                    matched_det_indices.insert(det_idx);
                    break;
                }
            }
        }
    }

    tracker_.Run(detection_rects);
    std::map<int, Track> tracks = tracker_.GetTracks();

    // New tracks created from unmatched detections inside Run(). Their Kalman
    // state is initialised directly from the detection rect, so IoU against the
    // source detection is 1.0 — the highest of any unused candidate.
    for (const auto& [track_id, track] : tracks) {
        if (pre_run_ids.count(track_id) > 0) {
            continue;
        }
        cv::Rect init_bbox = track.GetStateAsBbox();
        float best_iou = 0.0F;
        int best_det_idx = -1;
        for (int det_idx = 0; det_idx < static_cast<int>(detection_rects.size()); ++det_idx) {
            if (matched_det_indices.count(det_idx) > 0) {
                continue;
            }
            float iou = ComputeIoU(init_bbox, detection_rects[det_idx]);
            if (iou > best_iou) {
                best_iou = iou;
                best_det_idx = det_idx;
            }
        }
        if (best_det_idx >= 0) {
            track_to_det_idx[track_id] = best_det_idx;
            matched_det_indices.insert(best_det_idx);
        }
    }

    std::vector<TrackedDetection> tracked_detections;

    for (const auto& [track_id, track] : tracks) {
        bool is_active = (track.coast_cycles_ == 0);
        bool is_confirmed_active = is_active && (track.hit_streak_ >= min_hits_);
        bool is_sort_coasting = (track.coast_cycles_ == 1);

        if (!is_confirmed_active && !is_sort_coasting) {
            continue;
        }

        TrackedDetection tracked_detection;
        tracked_detection.track_id = track_id;

        auto match_iterator = track_to_det_idx.find(track_id);
        if (match_iterator != track_to_det_idx.end() && is_active) {
            int det_idx = match_iterator->second;
            tracked_detection.bbox = detections[det_idx].bbox;
            tracked_detection.confidence = detections[det_idx].confidence;
            tracked_detection.class_id = detections[det_idx].class_id;
        } else {
            tracked_detection.bbox = track.GetStateAsBbox();
            tracked_detection.confidence = 0.0F;
            tracked_detection.class_id = -1;
        }

        tracked_detections.push_back(tracked_detection);
    }

    return tracked_detections;
}
