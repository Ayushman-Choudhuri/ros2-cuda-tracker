#pragma once

#include <cuda_tracker/msg/tracked_detection_array.hpp>
#include <memory>
#include <opencv2/core.hpp>
#include <rclcpp/rclcpp.hpp>
#include <rclcpp_lifecycle/lifecycle_node.hpp>
#include <sensor_msgs/msg/image.hpp>
#include <std_msgs/msg/header.hpp>
#include <string>
#include <vector>

#include "camera/camera.hpp"
#include "inference/detector.hpp"
#include "tracker/tracker.hpp"

namespace vision {

    class TrackerNode : public rclcpp_lifecycle::LifecycleNode {
       public:
        using CallbackReturn =
            rclcpp_lifecycle::node_interfaces::LifecycleNodeInterface::CallbackReturn;

        explicit TrackerNode(const rclcpp::NodeOptions& options);

        CallbackReturn on_configure(const rclcpp_lifecycle::State& state) override;
        CallbackReturn on_activate(const rclcpp_lifecycle::State& state) override;
        CallbackReturn on_deactivate(const rclcpp_lifecycle::State& state) override;
        CallbackReturn on_cleanup(const rclcpp_lifecycle::State& state) override;
        CallbackReturn on_shutdown(const rclcpp_lifecycle::State& state) override;

       private:
        // No defaults here: the declared ROS parameters are the single source of truth.
        struct Parameters {
            std::string input_source;
            int camera_device_id;
            int camera_frame_width;
            int camera_frame_height;
            std::string image_topic;
            double publish_rate_hz;
            std::string frame_id;
            std::string engine_path;
            DetectorConfig detector;
            TrackerConfig tracker;
        };

        void DeclareParameters();
        [[nodiscard]] Parameters ReadParameters() const;
        [[nodiscard]] bool ParametersAreValid(const Parameters& parameters) const;

        void OnCameraTimer();
        void OnImage(const sensor_msgs::msg::Image::ConstSharedPtr& message);
        void ProcessFrame(const cv::Mat& frame, const std_msgs::msg::Header& header);
        void StopCapture();
        void ReleaseEverything();

        Parameters parameters_{};

        std::unique_ptr<Detector> detector_;
        std::unique_ptr<ObjectTracker> tracker_;
        std::unique_ptr<Camera> camera_;

        rclcpp::TimerBase::SharedPtr capture_timer_;
        rclcpp::Subscription<sensor_msgs::msg::Image>::SharedPtr image_subscription_;

        rclcpp_lifecycle::LifecyclePublisher<cuda_tracker::msg::TrackedDetectionArray>::SharedPtr
            tracks_publisher_;
    };

}  // namespace vision
