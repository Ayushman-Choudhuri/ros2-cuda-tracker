#include "node/tracker_node.hpp"

#include <cv_bridge/cv_bridge.h>

#include <chrono>
#include <exception>
#include <filesystem>
#include <lifecycle_msgs/msg/state.hpp>
#include <rclcpp_components/register_node_macro.hpp>

#include "utils/conversions.hpp"
#include "utils/logger.hpp"

namespace vision {
    namespace {

        // Pointers rather than arrays: these feed printf-style RCLCPP_* macros, and an
        // array form trips modernize-avoid-c-arrays.
        constexpr const char* kCameraSource = "camera";
        constexpr const char* kTopicSource = "topic";
        constexpr const char* kImageEncoding = "bgr8";
        constexpr int kWarnThrottleMs = 1000;

        // A node's cwd is arbitrary under `ros2 run`, `ros2 launch` and systemd, so a
        // relative path resolves against the repo the package was built from.
        std::string ResolveEnginePath(const std::string& configured_path) {
            const std::filesystem::path path(configured_path);
            if (path.is_absolute()) {
                return configured_path;
            }
            return (std::filesystem::path(CUDA_TRACKER_REPO_ROOT) / path).lexically_normal();
        }

    }  // namespace

    TrackerNode::TrackerNode(const rclcpp::NodeOptions& options)
        : rclcpp_lifecycle::LifecycleNode("tracker_node", options) {
        // The pipeline classes log through utils/logger.hpp, not /rosout, so their level
        // is not covered by the ROS logging configuration.
        Logger::SetLevelFromEnvironment();
        DeclareParameters();
    }

    void TrackerNode::DeclareParameters() {
        declare_parameter<std::string>("input_source", kCameraSource);
        declare_parameter<int>("camera.device_id", 4);
        declare_parameter<int>("camera.frame_width", 640);
        declare_parameter<int>("camera.frame_height", 480);
        declare_parameter<std::string>("image_topic", "image_raw");
        declare_parameter<double>("publish_rate_hz", 30.0);
        declare_parameter<std::string>("frame_id", "camera_optical_frame");
        declare_parameter<std::string>("engine_path", "models/engine/yolov10n_fp16.engine");
        declare_parameter<int>("model_input_size", 800);
        declare_parameter<double>("confidence_threshold", 0.1);
        declare_parameter<int>("target_class_id", 0);
        declare_parameter<int>("tracker.frame_rate", 30);
        declare_parameter<int>("tracker.track_buffer", 30);
        declare_parameter<double>("tracker.track_thresh", 0.5);
        declare_parameter<double>("tracker.high_thresh", 0.6);
        declare_parameter<double>("tracker.match_thresh", 0.8);
    }

    TrackerNode::Parameters TrackerNode::ReadParameters() const {
        Parameters parameters{};
        get_parameter("input_source", parameters.input_source);
        get_parameter("camera.device_id", parameters.camera_device_id);
        get_parameter("camera.frame_width", parameters.camera_frame_width);
        get_parameter("camera.frame_height", parameters.camera_frame_height);
        get_parameter("image_topic", parameters.image_topic);
        get_parameter("publish_rate_hz", parameters.publish_rate_hz);
        get_parameter("frame_id", parameters.frame_id);
        get_parameter("model_input_size", parameters.detector.input_size);
        get_parameter("confidence_threshold", parameters.detector.confidence_threshold);
        get_parameter("target_class_id", parameters.detector.target_class_id);
        get_parameter("tracker.frame_rate", parameters.tracker.frame_rate);
        get_parameter("tracker.track_buffer", parameters.tracker.track_buffer);
        get_parameter("tracker.track_thresh", parameters.tracker.track_thresh);
        get_parameter("tracker.high_thresh", parameters.tracker.high_thresh);
        get_parameter("tracker.match_thresh", parameters.tracker.match_thresh);

        parameters.engine_path = ResolveEnginePath(get_parameter("engine_path").as_string());
        return parameters;
    }

    bool TrackerNode::ParametersAreValid(const Parameters& parameters) const {
        if (parameters.input_source != kCameraSource && parameters.input_source != kTopicSource) {
            RCLCPP_ERROR(get_logger(),
                         "input_source must be '%s' or '%s', got '%s'",
                         kCameraSource,
                         kTopicSource,
                         parameters.input_source.c_str());
            return false;
        }
        if (parameters.publish_rate_hz <= 0.0) {
            RCLCPP_ERROR(get_logger(),
                         "publish_rate_hz must be positive, got %f",
                         parameters.publish_rate_hz);
            return false;
        }
        return true;
    }

    TrackerNode::CallbackReturn TrackerNode::on_configure(const rclcpp_lifecycle::State&) {
        parameters_ = ReadParameters();
        if (!ParametersAreValid(parameters_)) {
            return CallbackReturn::FAILURE;
        }

        try {
            detector_ = std::make_unique<Detector>(parameters_.engine_path, parameters_.detector);
        } catch (const std::exception& error) {
            RCLCPP_ERROR(get_logger(), "Detector construction failed: %s", error.what());
            return CallbackReturn::FAILURE;
        }

        tracker_ = std::make_unique<ObjectTracker>(parameters_.tracker);

        tracks_publisher_ = create_publisher<cuda_tracker::msg::TrackedDetectionArray>(
            "tracked_detections", rclcpp::SensorDataQoS());

        if (parameters_.input_source == kTopicSource) {
            image_subscription_ = create_subscription<sensor_msgs::msg::Image>(
                parameters_.image_topic,
                rclcpp::SensorDataQoS(),
                [this](const sensor_msgs::msg::Image::ConstSharedPtr message) {
                    OnImage(message);
                });
        }

        RCLCPP_INFO(get_logger(),
                    "Configured. source=%s engine=%s",
                    parameters_.input_source.c_str(),
                    parameters_.engine_path.c_str());
        return CallbackReturn::SUCCESS;
    }

    TrackerNode::CallbackReturn TrackerNode::on_activate(const rclcpp_lifecycle::State& state) {
        LifecycleNode::on_activate(state);

        if (parameters_.input_source != kCameraSource) {
            return CallbackReturn::SUCCESS;
        }

        try {
            camera_ = std::make_unique<Camera>(parameters_.camera_device_id,
                                               parameters_.camera_frame_width,
                                               parameters_.camera_frame_height);
        } catch (const std::exception& error) {
            RCLCPP_ERROR(get_logger(), "Camera open failed: %s", error.what());
            return CallbackReturn::FAILURE;
        }

        const std::chrono::duration<double> period(1.0 / parameters_.publish_rate_hz);
        capture_timer_ =
            create_wall_timer(std::chrono::duration_cast<std::chrono::nanoseconds>(period),
                              [this]() { OnCameraTimer(); });

        return CallbackReturn::SUCCESS;
    }

    TrackerNode::CallbackReturn TrackerNode::on_deactivate(const rclcpp_lifecycle::State& state) {
        StopCapture();
        LifecycleNode::on_deactivate(state);
        return CallbackReturn::SUCCESS;
    }

    void TrackerNode::StopCapture() {
        if (capture_timer_) {
            capture_timer_->cancel();
            capture_timer_.reset();
        }
        camera_.reset();
    }

    void TrackerNode::ReleaseEverything() {
        StopCapture();
        image_subscription_.reset();
        tracks_publisher_.reset();
        tracker_.reset();
        detector_.reset();
    }

    TrackerNode::CallbackReturn TrackerNode::on_cleanup(const rclcpp_lifecycle::State&) {
        ReleaseEverything();
        return CallbackReturn::SUCCESS;
    }

    TrackerNode::CallbackReturn TrackerNode::on_shutdown(const rclcpp_lifecycle::State&) {
        ReleaseEverything();
        return CallbackReturn::SUCCESS;
    }

    void TrackerNode::OnCameraTimer() {
        const cv::Mat frame = camera_->GetFrame();
        if (frame.empty()) {
            RCLCPP_WARN_THROTTLE(
                get_logger(), *get_clock(), kWarnThrottleMs, "Camera returned an empty frame");
            return;
        }

        std_msgs::msg::Header header;
        header.stamp = now();
        header.frame_id = parameters_.frame_id;

        ProcessFrame(frame, header);
    }

    void TrackerNode::OnImage(const sensor_msgs::msg::Image::ConstSharedPtr& message) {
        // The subscription exists from on_configure onward, but inference must not run
        // until the node is active.
        if (get_current_state().id() != lifecycle_msgs::msg::State::PRIMARY_STATE_ACTIVE) {
            return;
        }

        cv_bridge::CvImageConstPtr converted;
        try {
            converted = cv_bridge::toCvShare(message, kImageEncoding);
        } catch (const cv_bridge::Exception& error) {
            RCLCPP_ERROR_THROTTLE(get_logger(),
                                  *get_clock(),
                                  kWarnThrottleMs,
                                  "cv_bridge conversion failed: %s",
                                  error.what());
            return;
        }

        // Header copied verbatim: consumers need the stamp of the frame the tracks were
        // computed from, not the time they were published.
        ProcessFrame(converted->image, message->header);
    }

    void TrackerNode::ProcessFrame(const cv::Mat& frame, const std_msgs::msg::Header& header) {
        const std::vector<Detection> detections = detector_->Detect(frame);
        const std::vector<TrackedDetection> tracks = tracker_->Update(detections);

        if (tracks_publisher_ && tracks_publisher_->is_activated()) {
            tracks_publisher_->publish(ToTrackedDetectionArray(tracks, header));
        }
    }

}  // namespace vision

RCLCPP_COMPONENTS_REGISTER_NODE(vision::TrackerNode)
