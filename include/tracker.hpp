#pragma once

// sort-cpp hard-codes iou_threshold=0.3 (AssociateDetectionsToTrackers default) and
// kMaxCoastCycles=1 (utils.h); Tracker::Run() exposes no API to override either at
// runtime, so both parameters are inoperative here and have been removed.

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
    explicit SORTTracker(int min_hits = 1);
    ~SORTTracker() = default;

    std::vector<TrackedDetection> Update(const std::vector<Detection>& detections);

   private:
    Tracker tracker_;
    int min_hits_;
};
