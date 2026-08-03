#pragma once

#include <cuda_tracker/msg/tracked_detection_array.hpp>
#include <std_msgs/msg/header.hpp>
#include <vector>

#include "utils/detection_types.hpp"

namespace vision {

    // vision::TrackedDetection is the internal struct, cuda_tracker::msg::TrackedDetection
    // the same-named wire message.
    cuda_tracker::msg::TrackedDetectionArray ToTrackedDetectionArray(
        const std::vector<TrackedDetection>& tracks, const std_msgs::msg::Header& header);

}  // namespace vision
