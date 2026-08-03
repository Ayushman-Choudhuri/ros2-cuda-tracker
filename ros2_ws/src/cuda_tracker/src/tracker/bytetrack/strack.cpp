#include "bytetrack/strack.hpp"

namespace byte_track {
    namespace {

        // xyah: the (center_x, center_y, aspect_ratio, height) the filter measures.
        KalmanFilter::DetectBox RectToXyah(const Rect& rect) {
            KalmanFilter::DetectBox xyah;
            xyah << rect.CenterX(), rect.CenterY(), rect.AspectRatio(), rect.Height();
            return xyah;
        }

    }  // namespace

    STrack::STrack(const Rect& rect, float score, int label)
        : mean_(KalmanFilter::StateMean::Zero()),
          covariance_(KalmanFilter::StateCov::Zero()),
          rect_(rect),
          score_(score),
          label_(label) {
    }

    const Rect& STrack::GetRect() const {
        return rect_;
    }

    TrackState STrack::GetState() const {
        return state_;
    }

    bool STrack::IsActivated() const {
        return is_activated_;
    }

    float STrack::GetScore() const {
        return score_;
    }

    int STrack::GetLabel() const {
        return label_;
    }

    size_t STrack::GetTrackId() const {
        return track_id_;
    }

    size_t STrack::GetFrameId() const {
        return frame_id_;
    }

    size_t STrack::GetStartFrameId() const {
        return start_frame_id_;
    }

    void STrack::Activate(size_t frame_id, size_t track_id) {
        kalman_filter_.Initiate(mean_, covariance_, RectToXyah(rect_));
        SyncRectFromState();

        state_ = TrackState::kTracked;
        // Tracks born on the very first frame have nothing to corroborate them against,
        // so they are trusted immediately instead of waiting a frame.
        is_activated_ = frame_id == 1;
        track_id_ = track_id;
        frame_id_ = frame_id;
        start_frame_id_ = frame_id;
    }

    void STrack::ReActivate(const STrack& detection, size_t frame_id) {
        AdoptMeasurement(detection, frame_id);
    }

    void STrack::Update(const STrack& detection, size_t frame_id) {
        AdoptMeasurement(detection, frame_id);
    }

    void STrack::Predict() {
        if (state_ != TrackState::kTracked) {
            // A track that is not being observed should not keep growing or shrinking.
            mean_[KalmanFilter::kHeightVelocity] = 0;
        }
        kalman_filter_.Predict(mean_, covariance_);
    }

    void STrack::AdoptMeasurement(const STrack& detection, size_t frame_id) {
        kalman_filter_.Update(mean_, covariance_, RectToXyah(detection.GetRect()));
        SyncRectFromState();

        state_ = TrackState::kTracked;
        is_activated_ = true;
        score_ = detection.GetScore();
        label_ = detection.GetLabel();
        frame_id_ = frame_id;
    }

    void STrack::MarkAsLost() {
        state_ = TrackState::kLost;
    }

    void STrack::MarkAsRemoved() {
        state_ = TrackState::kRemoved;
    }

    void STrack::SyncRectFromState() {
        rect_ = Rect::FromXyah(mean_[KalmanFilter::kCenterX], mean_[KalmanFilter::kCenterY],
                               mean_[KalmanFilter::kAspectRatio], mean_[KalmanFilter::kHeight]);
    }

}  // namespace byte_track
