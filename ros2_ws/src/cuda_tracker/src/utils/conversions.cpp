#include "utils/conversions.hpp"

namespace vision {

    cuda_tracker::msg::TrackedDetectionArray ToTrackedDetectionArray(
        const std::vector<TrackedDetection>& tracks, const std_msgs::msg::Header& header) {
        cuda_tracker::msg::TrackedDetectionArray message;
        message.header = header;
        message.detections.reserve(tracks.size());

        for (const TrackedDetection& track : tracks) {
            cuda_tracker::msg::TrackedDetection detection;

            // cv::Rect is corner + extent, the message centre + extent. The 2.0F keeps
            // this in floating point, so a 5-wide box centres on 2.5.
            detection.center_x = static_cast<float>(track.bbox.x) + track.bbox.width / 2.0F;
            detection.center_y = static_cast<float>(track.bbox.y) + track.bbox.height / 2.0F;
            detection.width = static_cast<float>(track.bbox.width);
            detection.height = static_cast<float>(track.bbox.height);
            detection.track_id = track.track_id;
            detection.class_id = track.class_id;

            message.detections.push_back(detection);
        }

        return message;
    }

}  // namespace vision
