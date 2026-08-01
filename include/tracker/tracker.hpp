#pragma once

#include <opencv2/core.hpp>
#include <vector>

#include "inference/detector.hpp"
#include "tracker/bytetrack/byte_tracker.hpp"

namespace vision {

    struct TrackedDetection {
        cv::Rect bbox;
        float confidence = 0.0F;
        int class_id = -1;
        int track_id = -1;
    };

    class ObjectTracker {
       public:
        std::vector<TrackedDetection> Update(const std::vector<Detection>& detections);

       private:
        byte_track::ByteTracker tracker_;
    };

}
