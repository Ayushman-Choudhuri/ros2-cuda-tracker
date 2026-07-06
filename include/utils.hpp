#pragma once

#include <opencv2/core.hpp>
#include <string>
#include <vector>

#include "detector.hpp"
#include "tracker.hpp"

// Draws bounding boxes and class labels onto frame in-place.
// Falls back to COCO-80 class names if class_names is empty.
void DrawDetections(cv::Mat& frame, const std::vector<Detection>& detections,
                    const std::vector<std::string>& class_names = {});

// Draws tracked bounding boxes with track IDs. Coasted tracks (class_id == -1) show "#id" only.
// Falls back to COCO-80 class names if class_names is empty.
void DrawTrackedDetections(cv::Mat& frame, const std::vector<TrackedDetection>& tracks,
                           const std::vector<std::string>& class_names = {});

// Draws "FPS: X.X" overlay at top-left corner.
void DrawFps(cv::Mat& frame, double fps);
