#include <exception>
#include <opencv2/core/utils/logger.hpp>
#include <opencv2/highgui.hpp>
#include <string>
#include <vector>

#include "camera/camera.hpp"
#include "inference/detector.hpp"
#include "tracker/tracker.hpp"
#include "utils/logger.hpp"
#include "utils/visualization.hpp"

namespace {

    constexpr int kCameraDeviceId = 4;
    const std::string kEnginePath = "models/engine/yolov10x_fp16.engine";
    constexpr int kModelInputSize = 640;
    constexpr int kPersonClassId = 0;

    // ByteTrack's own thresholds decide what becomes a
    // track, and its second association pass needs the low-confidence detections a
    // stricter detector threshold would discard.
    constexpr float kDetectionConfidenceThreshold = 0.1F;

    const std::string kWindowName = "Person Tracking";
    constexpr int kEscapeKey = 27;
    constexpr int kKeyPollMs = 1;

    bool QuitRequested(int key) {
        return key == 'q' || key == 'Q' || key == kEscapeKey;
    }

    void RunTrackingLoop(vision::Camera& camera,
                         vision::Detector& detector,
                         vision::ObjectTracker& tracker) {
        cv::namedWindow(kWindowName, cv::WINDOW_AUTOSIZE);

        while (true) {
            cv::Mat frame = camera.GetFrame();

            if (frame.empty()) {
                LOG_ERROR("Main") << "Camera returned an empty frame, stopping.";
                return;
            }

            const std::vector<vision::Detection> detections = detector.Detect(frame);
            const std::vector<vision::TrackedDetection> tracks = tracker.Update(detections);

            vision::DrawTracks(frame, tracks);
            vision::DrawFps(frame, camera.GetFps());
            cv::imshow(kWindowName, frame);

            if (QuitRequested(cv::waitKey(kKeyPollMs))) {
                return;
            }
        }
    }

}

int main() {
    cv::utils::logging::setLogLevel(cv::utils::logging::LOG_LEVEL_SILENT);
    vision::Logger::SetLevelFromEnvironment();

    try {
        vision::Camera camera(kCameraDeviceId);
        vision::Detector detector(kEnginePath,
                                  kDetectionConfidenceThreshold,
                                  kModelInputSize,
                                  kModelInputSize,
                                  kPersonClassId);

        vision::ObjectTracker tracker;

        LOG_INFO("Main") << "Running. Press 'q' or ESC to quit.";

        RunTrackingLoop(camera, detector, tracker);
    }

    catch (const std::exception& error) {
        LOG_FATAL("Main") << error.what();
        return 1;
    }

    cv::destroyAllWindows();
    return 0;
}
