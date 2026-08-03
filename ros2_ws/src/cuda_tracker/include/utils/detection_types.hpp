#pragma once

#include <opencv2/core.hpp>

namespace vision {

    struct Detection {
        cv::Rect bbox;
        float confidence = 0.0F;
        int class_id = -1;
    };

    struct TrackedDetection {
        cv::Rect bbox;
        float confidence = 0.0F;
        int class_id = -1;
        int track_id = -1;
    };

}  // namespace vision
