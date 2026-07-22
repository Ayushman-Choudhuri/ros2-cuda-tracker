// Adapted from ByteTrack-cpp (https://github.com/Vertical-Beach/ByteTrack-cpp),
// MIT License. See third_party/ByteTrack-cpp.LICENSE. Reformatted to this
// repo's Google C++ style; tracking behaviour is unchanged. The class label is
// carried through the track so the caller can recover it on the output.

#include "bytetrack/strack.hpp"

#include <cstddef>

namespace byte_track {

namespace {

// Build the (centre_x, centre_y, aspect_ratio, height) measurement vector the
// Kalman filter expects from a box in top-left/width/height form.
KalmanFilter::DetectBox RectToXyah(const Rect& rect) {
    KalmanFilter::DetectBox xyah;
    xyah << rect.CenterX(), rect.CenterY(), rect.AspectRatio(), rect.Height();
    return xyah;
}

}  // namespace

STrack::STrack(const Rect& rect, float score, int label)
    : kalman_filter_(),
      mean_(),
      covariance_(),
      rect_(rect),
      state_(STrackState::kNew),
      is_activated_(false),
      score_(score),
      label_(label),
      track_id_(0),
      frame_id_(0),
      start_frame_id_(0),
      tracklet_len_(0) {
}

STrack::~STrack() = default;

const Rect& STrack::GetRect() const {
    return rect_;
}
STrackState STrack::GetState() const {
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
size_t STrack::GetTrackletLength() const {
    return tracklet_len_;
}

void STrack::Activate(size_t frame_id, size_t track_id) {
    kalman_filter_.Initiate(mean_, covariance_, RectToXyah(rect_));

    UpdateRect();

    state_ = STrackState::kTracked;
    if (frame_id == 1) {
        is_activated_ = true;
    }
    track_id_ = track_id;
    frame_id_ = frame_id;
    start_frame_id_ = frame_id;
    tracklet_len_ = 0;
}

void STrack::ReActivate(const STrack& new_track, size_t frame_id, int new_track_id) {
    kalman_filter_.Update(mean_, covariance_, RectToXyah(new_track.GetRect()));

    UpdateRect();

    state_ = STrackState::kTracked;
    is_activated_ = true;
    score_ = new_track.GetScore();
    label_ = new_track.GetLabel();
    if (0 <= new_track_id) {
        track_id_ = new_track_id;
    }
    frame_id_ = frame_id;
    tracklet_len_ = 0;
}

void STrack::Predict() {
    if (state_ != STrackState::kTracked) {
        mean_[7] = 0;
    }
    kalman_filter_.Predict(mean_, covariance_);
}

void STrack::Update(const STrack& new_track, size_t frame_id) {
    kalman_filter_.Update(mean_, covariance_, RectToXyah(new_track.GetRect()));

    UpdateRect();

    state_ = STrackState::kTracked;
    is_activated_ = true;
    score_ = new_track.GetScore();
    label_ = new_track.GetLabel();
    frame_id_ = frame_id;
    tracklet_len_++;
}

void STrack::MarkAsLost() {
    state_ = STrackState::kLost;
}
void STrack::MarkAsRemoved() {
    state_ = STrackState::kRemoved;
}

void STrack::UpdateRect() {
    rect_ = Rect::FromXyah(mean_[0], mean_[1], mean_[2], mean_[3]);
}

}  // namespace byte_track
