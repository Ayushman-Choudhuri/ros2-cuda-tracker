#pragma once

#include <vector>

#include "tracker/bytetrack/byte_tracker.hpp"
#include "utils/detection_types.hpp"

namespace vision {

    struct TrackerConfig {
        int frame_rate = 30;
        int track_buffer = 30;
        float track_thresh = 0.5F;
        float high_thresh = 0.6F;
        float match_thresh = 0.8F;
    };

    class ObjectTracker {
       public:
        explicit ObjectTracker(const TrackerConfig& config)
            : tracker_(config.frame_rate,
                       config.track_buffer,
                       config.track_thresh,
                       config.high_thresh,
                       config.match_thresh) {}

        std::vector<TrackedDetection> Update(const std::vector<Detection>& detections);

       private:
        byte_track::ByteTracker tracker_;
    };

}  // namespace vision
