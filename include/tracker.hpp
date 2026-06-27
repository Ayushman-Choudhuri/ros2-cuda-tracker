#pragma once

#include <vector>

#include <opencv2/core.hpp>

#include "detector.hpp"
#include "tracker.h"

struct TrackedDetection {
    cv::Rect bbox;
    float confidence;
    int class_id;   // -1 for coasted tracks with no current detection match
    int track_id;
};

class SORTTracker {
   public:
    explicit SORTTracker(int max_age = 200, int min_hits = 1, float iou_threshold = 0.5F);
    ~SORTTracker() = default;

    std::vector<TrackedDetection> Update(const std::vector<Detection>& detections);

   private:
    Tracker tracker_;
    int max_age_;
    int min_hits_;
    float iou_threshold_;
};
