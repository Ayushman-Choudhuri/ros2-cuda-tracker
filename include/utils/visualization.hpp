#pragma once

#include <opencv2/core.hpp>
#include <vector>

#include "tracker/tracker.hpp"

namespace vision {

    void DrawTracks(cv::Mat& frame, const std::vector<TrackedDetection>& tracks);

    void DrawFps(cv::Mat& frame, double fps);

}
