// Adapted from ByteTrack-cpp (https://github.com/Vertical-Beach/ByteTrack-cpp),
// MIT License. See LICENSE in this directory. The association pipeline was split
// into one function per stage and the raw LAPJV buffers replaced with owning
// containers; the tracking result is unchanged.

#include "bytetrack/byte_tracker.hpp"

#include <map>
#include <memory>
#include <numeric>
#include <stdexcept>
#include <unordered_set>
#include <utility>

#include "bytetrack/lapjv.hpp"

namespace byte_track {
    namespace {

        // Association thresholds fixed by the paper rather than exposed as knobs.
        constexpr float kLowScoreMatchThresh = 0.5F;
        constexpr float kUnconfirmedMatchThresh = 0.7F;

        constexpr float kDuplicateIouDistance = 0.15F;

        constexpr double kReferenceFrameRate = 30.0;

        std::vector<std::shared_ptr<STrack>> DropMarked(
            const std::vector<std::shared_ptr<STrack>>& tracks,
            const std::vector<bool>& is_marked) {
            std::vector<std::shared_ptr<STrack>> kept;
            kept.reserve(tracks.size());
            for (size_t index = 0; index < tracks.size(); ++index) {
                if (!is_marked[index]) {
                    kept.push_back(tracks[index]);
                }
            }
            return kept;
        }

    }  // namespace

    ByteTracker::ByteTracker(int frame_rate, int track_buffer, float track_thresh,
                             float high_thresh, float match_thresh)
        : track_thresh_(track_thresh),
          high_thresh_(high_thresh),
          match_thresh_(match_thresh),
          max_time_lost_(static_cast<size_t>(frame_rate / kReferenceFrameRate * track_buffer)) {
    }

    std::vector<ByteTracker::STrackPtr> ByteTracker::Update(const std::vector<Object>& objects) {
        frame_id_++;

        std::vector<STrackPtr> high_score_detections;
        std::vector<STrackPtr> low_score_detections;
        SplitDetectionsByScore(objects, high_score_detections, low_score_detections);

        std::vector<STrackPtr> confirmed_tracks;
        std::vector<STrackPtr> unconfirmed_tracks;
        for (const auto& track : tracked_stracks_) {
            (track->IsActivated() ? confirmed_tracks : unconfirmed_tracks).push_back(track);
        }
        const std::vector<STrackPtr> track_pool = PredictedTrackPool(confirmed_tracks);

        FrameTracks frame_tracks;
        std::vector<STrackPtr> unmatched_tracks;
        std::vector<STrackPtr> unmatched_detections;
        MatchHighScoreDetections(track_pool, high_score_detections, frame_tracks, unmatched_tracks,
                                 unmatched_detections);
        MatchLowScoreDetections(unmatched_tracks, low_score_detections, frame_tracks);
        StartNewTracks(unconfirmed_tracks, unmatched_detections, frame_tracks);
        ExpireLostTracks(frame_tracks);
        CommitFrameTracks(frame_tracks);

        std::vector<STrackPtr> output_tracks;
        for (const auto& track : tracked_stracks_) {
            if (track->IsActivated()) {
                output_tracks.push_back(track);
            }
        }
        return output_tracks;
    }

    void ByteTracker::SplitDetectionsByScore(const std::vector<Object>& objects,
                                             std::vector<STrackPtr>& high_score,
                                             std::vector<STrackPtr>& low_score) const {
        for (const auto& object : objects) {
            auto detection = std::make_shared<STrack>(object.rect, object.score, object.label);
            if (object.score >= track_thresh_) {
                high_score.push_back(std::move(detection));
            } else {
                low_score.push_back(std::move(detection));
            }
        }
    }

    std::vector<ByteTracker::STrackPtr> ByteTracker::PredictedTrackPool(
        const std::vector<STrackPtr>& confirmed_tracks) const {
        std::vector<STrackPtr> track_pool = JoinTracks(confirmed_tracks, lost_stracks_);
        for (const auto& track : track_pool) {
            track->Predict();
        }
        return track_pool;
    }

    void ByteTracker::MatchHighScoreDetections(const std::vector<STrackPtr>& track_pool,
                                               const std::vector<STrackPtr>& detections,
                                               FrameTracks& frame_tracks,
                                               std::vector<STrackPtr>& unmatched_tracks,
                                               std::vector<STrackPtr>& unmatched_detections) const {
        const Association association = Associate(track_pool, detections, match_thresh_);
        ApplyMatches(track_pool, detections, association.matches, frame_tracks);

        for (size_t detection_index : association.unmatched_detections) {
            unmatched_detections.push_back(detections[detection_index]);
        }
        for (size_t track_index : association.unmatched_tracks) {
            if (track_pool[track_index]->GetState() == TrackState::kTracked) {
                unmatched_tracks.push_back(track_pool[track_index]);
            }
        }
    }

    void ByteTracker::MatchLowScoreDetections(const std::vector<STrackPtr>& tracks,
                                              const std::vector<STrackPtr>& detections,
                                              FrameTracks& frame_tracks) const {
        const Association association = Associate(tracks, detections, kLowScoreMatchThresh);
        ApplyMatches(tracks, detections, association.matches, frame_tracks);

        for (size_t track_index : association.unmatched_tracks) {
            const STrackPtr& track = tracks[track_index];
            if (track->GetState() != TrackState::kLost) {
                track->MarkAsLost();
                frame_tracks.lost.push_back(track);
            }
        }
    }

    void ByteTracker::StartNewTracks(const std::vector<STrackPtr>& unconfirmed_tracks,
                                     const std::vector<STrackPtr>& detections,
                                     FrameTracks& frame_tracks) {
        const Association association =
            Associate(unconfirmed_tracks, detections, kUnconfirmedMatchThresh);

        for (const auto& [track_index, detection_index] : association.matches) {
            unconfirmed_tracks[track_index]->Update(*detections[detection_index], frame_id_);
            frame_tracks.activated.push_back(unconfirmed_tracks[track_index]);
        }

        // An unconfirmed track is a single unsupported detection from the previous
        // frame; without a second sighting it is treated as noise.
        for (size_t track_index : association.unmatched_tracks) {
            const STrackPtr& track = unconfirmed_tracks[track_index];
            track->MarkAsRemoved();
            frame_tracks.removed.push_back(track);
        }

        for (size_t detection_index : association.unmatched_detections) {
            const STrackPtr& detection = detections[detection_index];
            if (detection->GetScore() < high_thresh_) {
                continue;
            }
            track_id_count_++;
            detection->Activate(frame_id_, track_id_count_);
            frame_tracks.activated.push_back(detection);
        }
    }

    void ByteTracker::ExpireLostTracks(FrameTracks& frame_tracks) const {
        for (const auto& track : lost_stracks_) {
            if (frame_id_ - track->GetFrameId() > max_time_lost_) {
                track->MarkAsRemoved();
                frame_tracks.removed.push_back(track);
            }
        }
    }

    void ByteTracker::CommitFrameTracks(const FrameTracks& frame_tracks) {
        tracked_stracks_ = JoinTracks(frame_tracks.activated, frame_tracks.refound);
        lost_stracks_ = SubtractTracks(
            JoinTracks(SubtractTracks(lost_stracks_, tracked_stracks_), frame_tracks.lost),
            removed_stracks_);
        removed_stracks_ = JoinTracks(removed_stracks_, frame_tracks.removed);
        RemoveDuplicateTracks(tracked_stracks_, lost_stracks_);
    }

    void ByteTracker::ApplyMatches(const std::vector<STrackPtr>& tracks,
                                   const std::vector<STrackPtr>& detections,
                                   const std::vector<Match>& matches,
                                   FrameTracks& frame_tracks) const {
        for (const auto& [track_index, detection_index] : matches) {
            const STrackPtr& track = tracks[track_index];
            const STrackPtr& detection = detections[detection_index];
            if (track->GetState() == TrackState::kTracked) {
                track->Update(*detection, frame_id_);
                frame_tracks.activated.push_back(track);
            } else {
                track->ReActivate(*detection, frame_id_);
                frame_tracks.refound.push_back(track);
            }
        }
    }

    ByteTracker::Association ByteTracker::Associate(const std::vector<STrackPtr>& tracks,
                                                    const std::vector<STrackPtr>& detections,
                                                    float match_thresh) {
        return SolveAssignment(CalcIouDistances(tracks, detections), tracks.size(),
                               detections.size(), match_thresh);
    }

    std::vector<std::vector<float>> ByteTracker::CalcIouDistances(
        const std::vector<STrackPtr>& tracks, const std::vector<STrackPtr>& detections) {
        if (tracks.empty() || detections.empty()) {
            return {};
        }

        std::vector<std::vector<float>> distances(tracks.size(),
                                                  std::vector<float>(detections.size()));
        for (size_t track_index = 0; track_index < tracks.size(); ++track_index) {
            for (size_t detection_index = 0; detection_index < detections.size();
                 ++detection_index) {
                distances[track_index][detection_index] =
                    1.0F -
                    tracks[track_index]->GetRect().CalcIoU(detections[detection_index]->GetRect());
            }
        }
        return distances;
    }

    // The LAPJV kernel solves square problems only. A rectangular cost matrix is
    // padded to (track_count + detection_count) squared, where the padding costs
    // `cost_limit / 2` each: a pair whose real cost exceeds `cost_limit` is then
    // cheaper to leave unassigned via two padding cells than to match.
    ByteTracker::Association ByteTracker::SolveAssignment(
        const std::vector<std::vector<float>>& cost_matrix, size_t track_count,
        size_t detection_count, float cost_limit) {
        Association association;

        if (cost_matrix.empty()) {
            association.unmatched_tracks.resize(track_count);
            std::iota(association.unmatched_tracks.begin(), association.unmatched_tracks.end(), 0);
            association.unmatched_detections.resize(detection_count);
            std::iota(association.unmatched_detections.begin(),
                      association.unmatched_detections.end(), 0);
            return association;
        }

        const size_t matrix_size = track_count + detection_count;
        std::vector<double> padded_costs(matrix_size * matrix_size, cost_limit / 2.0);
        for (size_t row = track_count; row < matrix_size; ++row) {
            for (size_t column = detection_count; column < matrix_size; ++column) {
                padded_costs[row * matrix_size + column] = 0.0;
            }
        }
        for (size_t row = 0; row < track_count; ++row) {
            for (size_t column = 0; column < detection_count; ++column) {
                padded_costs[row * matrix_size + column] = cost_matrix[row][column];
            }
        }

        std::vector<double*> rows(matrix_size);
        for (size_t row = 0; row < matrix_size; ++row) {
            rows[row] = padded_costs.data() + row * matrix_size;
        }

        std::vector<int> track_to_detection(matrix_size);
        std::vector<int> detection_to_track(matrix_size);
        if (lapjv_internal(matrix_size, rows.data(), track_to_detection.data(),
                           detection_to_track.data()) != 0) {
            throw std::runtime_error("LAPJV assignment solver failed");
        }

        const auto is_real = [](int index, size_t count) {
            return index >= 0 && static_cast<size_t>(index) < count;
        };

        for (size_t track_index = 0; track_index < track_count; ++track_index) {
            const int detection_index = track_to_detection[track_index];
            if (is_real(detection_index, detection_count)) {
                association.matches.emplace_back(track_index, static_cast<size_t>(detection_index));
            } else {
                association.unmatched_tracks.push_back(track_index);
            }
        }
        for (size_t detection_index = 0; detection_index < detection_count; ++detection_index) {
            if (!is_real(detection_to_track[detection_index], track_count)) {
                association.unmatched_detections.push_back(detection_index);
            }
        }
        return association;
    }

    std::vector<ByteTracker::STrackPtr> ByteTracker::JoinTracks(
        const std::vector<STrackPtr>& first, const std::vector<STrackPtr>& second) {
        std::vector<STrackPtr> joined = first;
        joined.reserve(first.size() + second.size());

        std::unordered_set<size_t> seen_track_ids;
        for (const auto& track : first) {
            seen_track_ids.insert(track->GetTrackId());
        }
        for (const auto& track : second) {
            if (seen_track_ids.insert(track->GetTrackId()).second) {
                joined.push_back(track);
            }
        }
        return joined;
    }

    std::vector<ByteTracker::STrackPtr> ByteTracker::SubtractTracks(
        const std::vector<STrackPtr>& from, const std::vector<STrackPtr>& remove) {
        std::map<size_t, STrackPtr> remaining;
        for (const auto& track : from) {
            remaining.emplace(track->GetTrackId(), track);
        }
        for (const auto& track : remove) {
            remaining.erase(track->GetTrackId());
        }

        std::vector<STrackPtr> result;
        result.reserve(remaining.size());
        for (const auto& [track_id, track] : remaining) {
            result.push_back(track);
        }
        return result;
    }

    // A target briefly split into two tracks leaves near-identical boxes in the tracked
    // and lost lists. Keep whichever has been alive longer.
    void ByteTracker::RemoveDuplicateTracks(std::vector<STrackPtr>& tracked,
                                            std::vector<STrackPtr>& lost) {
        const std::vector<std::vector<float>> distances = CalcIouDistances(tracked, lost);

        std::vector<bool> tracked_is_duplicate(tracked.size(), false);
        std::vector<bool> lost_is_duplicate(lost.size(), false);
        for (size_t tracked_index = 0; tracked_index < distances.size(); ++tracked_index) {
            for (size_t lost_index = 0; lost_index < distances[tracked_index].size();
                 ++lost_index) {
                if (distances[tracked_index][lost_index] >= kDuplicateIouDistance) {
                    continue;
                }
                const size_t tracked_age = tracked[tracked_index]->GetFrameId() -
                                           tracked[tracked_index]->GetStartFrameId();
                const size_t lost_age =
                    lost[lost_index]->GetFrameId() - lost[lost_index]->GetStartFrameId();
                if (tracked_age > lost_age) {
                    lost_is_duplicate[lost_index] = true;
                } else {
                    tracked_is_duplicate[tracked_index] = true;
                }
            }
        }

        tracked = DropMarked(tracked, tracked_is_duplicate);
        lost = DropMarked(lost, lost_is_duplicate);
    }

}  // namespace byte_track
