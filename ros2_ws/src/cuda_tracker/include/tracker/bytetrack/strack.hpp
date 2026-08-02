#pragma once

#include <cstddef>

#include "bytetrack/kalman_filter.hpp"
#include "bytetrack/rect.hpp"

namespace byte_track {

    enum class TrackState {
        kNew,
        kTracked,
        kLost,
        kRemoved,
    };

    class STrack {
       public:
        STrack(const Rect& rect, float score, int label);

        [[nodiscard]] const Rect& GetRect() const;
        [[nodiscard]] TrackState GetState() const;
        [[nodiscard]] bool IsActivated() const;
        [[nodiscard]] float GetScore() const;
        [[nodiscard]] int GetLabel() const;
        [[nodiscard]] size_t GetTrackId() const;
        [[nodiscard]] size_t GetFrameId() const;
        [[nodiscard]] size_t GetStartFrameId() const;

        void Activate(size_t frame_id, size_t track_id);
        void ReActivate(const STrack& detection, size_t frame_id);
        void Predict();
        void Update(const STrack& detection, size_t frame_id);

        void MarkAsLost();
        void MarkAsRemoved();

       private:
        void AdoptMeasurement(const STrack& detection, size_t frame_id);
        void SyncRectFromState();

        KalmanFilter kalman_filter_;
        KalmanFilter::StateMean mean_;
        KalmanFilter::StateCov covariance_;

        Rect rect_;
        TrackState state_ = TrackState::kNew;

        bool is_activated_ = false;
        float score_;
        int label_;
        size_t track_id_ = 0;
        size_t frame_id_ = 0;
        size_t start_frame_id_ = 0;
    };

}  // namespace byte_track
