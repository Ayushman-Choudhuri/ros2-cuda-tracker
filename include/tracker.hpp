#pragma once

#include <opencv2/core.hpp>
#include <vector>

#include "bytetrack/byte_tracker.hpp"
#include "detector.hpp"

struct TrackedDetection {
    cv::Rect bbox;
    float confidence;
    int class_id;
    int track_id;
};

// Thin adapter over byte_track::ByteTracker: converts the pipeline's Detection
// type in and TrackedDetection out. ByteTrack returns only activated tracks, so
// every entry carries a live class_id and confidence (no coasted placeholders).
class ByteTracker {
   public:
    explicit ByteTracker(int frame_rate = 30, int track_buffer = 30, float track_thresh = 0.5F,
                         float high_thresh = 0.6F, float match_thresh = 0.8F);
    ~ByteTracker() = default;

    std::vector<TrackedDetection> Update(const std::vector<Detection>& detections);

   private:
    byte_track::ByteTracker tracker_;
};
