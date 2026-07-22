#pragma once

// Adapted from ByteTrack-cpp (https://github.com/Vertical-Beach/ByteTrack-cpp),
// MIT License. See third_party/ByteTrack-cpp.LICENSE. Reformatted to this
// repo's Google C++ style; tracking behaviour is unchanged.

#include <cstddef>
#include <limits>
#include <memory>
#include <vector>

#include "bytetrack/object.hpp"
#include "bytetrack/strack.hpp"

namespace byte_track {

// ByteTrack multi-object tracker: associates high- and low-confidence
// detections against existing tracks over two IoU matching passes, keeping
// lost tracks alive for a bounded number of frames before removing them.
class ByteTracker {
   public:
    using STrackPtr = std::shared_ptr<STrack>;

    ByteTracker(int frame_rate = 30, int track_buffer = 30, float track_thresh = 0.5F,
                float high_thresh = 0.6F, float match_thresh = 0.8F);
    ~ByteTracker();

    std::vector<STrackPtr> Update(const std::vector<Object>& objects);

   private:
    // These helpers are stateless; kept as private statics so callers stay
    // scoped to the tracker.
    [[nodiscard]] static std::vector<STrackPtr> JointStracks(
        const std::vector<STrackPtr>& first_track_list,
        const std::vector<STrackPtr>& second_track_list);

    [[nodiscard]] static std::vector<STrackPtr> SubStracks(
        const std::vector<STrackPtr>& first_track_list,
        const std::vector<STrackPtr>& second_track_list);

    static void RemoveDuplicateStracks(const std::vector<STrackPtr>& a_stracks,
                                       const std::vector<STrackPtr>& b_stracks,
                                       std::vector<STrackPtr>& a_result,
                                       std::vector<STrackPtr>& b_result);

    static void LinearAssignment(const std::vector<std::vector<float>>& cost_matrix, int num_rows,
                                 int num_cols, float thresh, std::vector<std::vector<int>>& matches,
                                 std::vector<int>& unmatched_rows,
                                 std::vector<int>& unmatched_cols);

    [[nodiscard]] static std::vector<std::vector<float>> CalcIouDistance(
        const std::vector<STrackPtr>& a_tracks, const std::vector<STrackPtr>& b_tracks);

    [[nodiscard]] static std::vector<std::vector<float>> CalcIous(const std::vector<Rect>& a_rects,
                                                                  const std::vector<Rect>& b_rects);

    static double ExecLapjv(const std::vector<std::vector<float>>& cost, std::vector<int>& rowsol,
                            std::vector<int>& colsol, bool extend_cost = false,
                            float cost_limit = std::numeric_limits<float>::max(),
                            bool return_cost = true);

    const float track_thresh_;
    const float high_thresh_;
    const float match_thresh_;
    const size_t max_time_lost_;

    size_t frame_id_;
    size_t track_id_count_;

    std::vector<STrackPtr> tracked_stracks_;
    std::vector<STrackPtr> lost_stracks_;
    std::vector<STrackPtr> removed_stracks_;
};

}  // namespace byte_track
