#pragma once

// Adapted from ByteTrack-cpp (https://github.com/Vertical-Beach/ByteTrack-cpp),
// MIT License. See third_party/ByteTrack-cpp.LICENSE. Reformatted to this
// repo's Google C++ style; tracking behaviour is unchanged. The class label is
// carried through the track so the caller can recover it on the output.

#include <cstddef>

#include "bytetrack/kalman_filter.hpp"
#include "bytetrack/rect.hpp"

namespace byte_track {

enum class STrackState {
    kNew = 0,
    kTracked = 1,
    kLost = 2,
    kRemoved = 3,
};

// A single tracklet: its Kalman state, book-keeping about how long it has been
// alive/lost, and the class label of the detection that spawned it.
class STrack {
   public:
    STrack(const Rect& rect, float score, int label);
    ~STrack();

    [[nodiscard]] const Rect& GetRect() const;
    [[nodiscard]] STrackState GetState() const;

    [[nodiscard]] bool IsActivated() const;
    [[nodiscard]] float GetScore() const;
    [[nodiscard]] int GetLabel() const;
    [[nodiscard]] size_t GetTrackId() const;
    [[nodiscard]] size_t GetFrameId() const;
    [[nodiscard]] size_t GetStartFrameId() const;
    [[nodiscard]] size_t GetTrackletLength() const;

    void Activate(size_t frame_id, size_t track_id);
    void ReActivate(const STrack& new_track, size_t frame_id, int new_track_id = -1);

    void Predict();
    void Update(const STrack& new_track, size_t frame_id);

    void MarkAsLost();
    void MarkAsRemoved();

   private:
    KalmanFilter kalman_filter_;
    KalmanFilter::StateMean mean_;
    KalmanFilter::StateCov covariance_;

    Rect rect_;
    STrackState state_;

    bool is_activated_;
    float score_;
    int label_;
    size_t track_id_;
    size_t frame_id_;
    size_t start_frame_id_;
    size_t tracklet_len_;

    void UpdateRect();
};

}  // namespace byte_track
