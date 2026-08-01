#pragma once

#include <cstddef>
#include <memory>
#include <utility>
#include <vector>

#include "bytetrack/object.hpp"
#include "bytetrack/strack.hpp"

namespace byte_track {

    class ByteTracker {
       public:
        using STrackPtr = std::shared_ptr<STrack>;

        explicit ByteTracker(int frame_rate = 30, int track_buffer = 30, float track_thresh = 0.5F,
                             float high_thresh = 0.6F, float match_thresh = 0.8F);

        std::vector<STrackPtr> Update(const std::vector<Object>& objects);

       private:
        using Match = std::pair<size_t, size_t>;

        struct Association {
            std::vector<Match> matches;
            std::vector<size_t> unmatched_tracks;
            std::vector<size_t> unmatched_detections;
        };

        struct FrameTracks {
            std::vector<STrackPtr> activated;
            std::vector<STrackPtr> refound;
            std::vector<STrackPtr> lost;
            std::vector<STrackPtr> removed;
        };

        void SplitDetectionsByScore(const std::vector<Object>& objects,
                                    std::vector<STrackPtr>& high_score,
                                    std::vector<STrackPtr>& low_score) const;

        [[nodiscard]] std::vector<STrackPtr> PredictedTrackPool(
            const std::vector<STrackPtr>& confirmed_tracks) const;

        void MatchHighScoreDetections(const std::vector<STrackPtr>& track_pool,
                                      const std::vector<STrackPtr>& detections,
                                      FrameTracks& frame_tracks,
                                      std::vector<STrackPtr>& unmatched_tracks,
                                      std::vector<STrackPtr>& unmatched_detections) const;

        void MatchLowScoreDetections(const std::vector<STrackPtr>& tracks,
                                     const std::vector<STrackPtr>& detections,
                                     FrameTracks& frame_tracks) const;

        void StartNewTracks(const std::vector<STrackPtr>& unconfirmed_tracks,
                            const std::vector<STrackPtr>& detections, FrameTracks& frame_tracks);

        void ExpireLostTracks(FrameTracks& frame_tracks) const;

        void CommitFrameTracks(const FrameTracks& frame_tracks);

        void ApplyMatches(const std::vector<STrackPtr>& tracks,
                          const std::vector<STrackPtr>& detections,
                          const std::vector<Match>& matches, FrameTracks& frame_tracks) const;

        [[nodiscard]] static Association Associate(const std::vector<STrackPtr>& tracks,
                                                   const std::vector<STrackPtr>& detections,
                                                   float match_thresh);

        [[nodiscard]] static std::vector<std::vector<float>> CalcIouDistances(
            const std::vector<STrackPtr>& tracks, const std::vector<STrackPtr>& detections);

        [[nodiscard]] static Association SolveAssignment(
            const std::vector<std::vector<float>>& cost_matrix, size_t track_count,
            size_t detection_count, float cost_limit);

        [[nodiscard]] static std::vector<STrackPtr> JoinTracks(
            const std::vector<STrackPtr>& first, const std::vector<STrackPtr>& second);

        [[nodiscard]] static std::vector<STrackPtr> SubtractTracks(
            const std::vector<STrackPtr>& from, const std::vector<STrackPtr>& remove);

        static void RemoveDuplicateTracks(std::vector<STrackPtr>& tracked,
                                          std::vector<STrackPtr>& lost);

        const float track_thresh_;
        const float high_thresh_;
        const float match_thresh_;
        const size_t max_time_lost_;

        size_t frame_id_ = 0;
        size_t track_id_count_ = 0;

        std::vector<STrackPtr> tracked_stracks_;
        std::vector<STrackPtr> lost_stracks_;
        std::vector<STrackPtr> removed_stracks_;
    };

}
