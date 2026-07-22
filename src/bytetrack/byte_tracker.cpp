// Adapted from ByteTrack-cpp (https://github.com/Vertical-Beach/ByteTrack-cpp),
// MIT License. See third_party/ByteTrack-cpp.LICENSE. Reformatted to this
// repo's Google C++ style; tracking behaviour is unchanged.

#include "bytetrack/byte_tracker.hpp"

#include <cstddef>
#include <limits>
#include <map>
#include <memory>
#include <stdexcept>
#include <utility>
#include <vector>

#include "bytetrack/lapjv.hpp"

namespace byte_track {

ByteTracker::ByteTracker(int frame_rate, int track_buffer, float track_thresh, float high_thresh,
                         float match_thresh)
    : track_thresh_(track_thresh),
      high_thresh_(high_thresh),
      match_thresh_(match_thresh),
      max_time_lost_(static_cast<size_t>(frame_rate / 30.0 * track_buffer)),
      frame_id_(0),
      track_id_count_(0) {
}

ByteTracker::~ByteTracker() = default;

std::vector<ByteTracker::STrackPtr> ByteTracker::Update(const std::vector<Object>& objects) {
    ////////////////// Step 1: Get detections //////////////////
    frame_id_++;

    // Create new STracks using the result of object detection
    std::vector<STrackPtr> det_stracks;
    std::vector<STrackPtr> det_low_stracks;

    for (const auto& object : objects) {
        auto strack = std::make_shared<STrack>(object.rect, object.prob, object.label);
        if (object.prob >= track_thresh_) {
            det_stracks.push_back(strack);
        } else {
            det_low_stracks.push_back(strack);
        }
    }

    // Create lists of existing STrack
    std::vector<STrackPtr> active_stracks;
    std::vector<STrackPtr> non_active_stracks;
    std::vector<STrackPtr> strack_pool;

    for (const auto& tracked_strack : tracked_stracks_) {
        if (!tracked_strack->IsActivated()) {
            non_active_stracks.push_back(tracked_strack);
        } else {
            active_stracks.push_back(tracked_strack);
        }
    }

    strack_pool = JointStracks(active_stracks, lost_stracks_);

    // Predict current pose by KF
    for (auto& strack : strack_pool) {
        strack->Predict();
    }

    ////////////////// Step 2: First association, with IoU //////////////////
    std::vector<STrackPtr> current_tracked_stracks;
    std::vector<STrackPtr> remain_tracked_stracks;
    std::vector<STrackPtr> remain_det_stracks;
    std::vector<STrackPtr> refind_stracks;

    {
        std::vector<std::vector<int>> matches_idx;
        std::vector<int> unmatch_detection_idx;
        std::vector<int> unmatch_track_idx;

        auto dists = CalcIouDistance(strack_pool, det_stracks);
        LinearAssignment(dists, strack_pool.size(), det_stracks.size(), match_thresh_, matches_idx,
                         unmatch_track_idx, unmatch_detection_idx);

        for (const auto& match_idx : matches_idx) {
            const auto& track = strack_pool[match_idx[0]];
            const auto& det = det_stracks[match_idx[1]];
            if (track->GetState() == STrackState::kTracked) {
                track->Update(*det, frame_id_);
                current_tracked_stracks.push_back(track);
            } else {
                track->ReActivate(*det, frame_id_);
                refind_stracks.push_back(track);
            }
        }

        for (const auto& unmatch_idx : unmatch_detection_idx) {
            remain_det_stracks.push_back(det_stracks[unmatch_idx]);
        }

        for (const auto& unmatch_idx : unmatch_track_idx) {
            if (strack_pool[unmatch_idx]->GetState() == STrackState::kTracked) {
                remain_tracked_stracks.push_back(strack_pool[unmatch_idx]);
            }
        }
    }

    ////////////////// Step 3: Second association, using low score dets //////////////////
    std::vector<STrackPtr> current_lost_stracks;

    {
        std::vector<std::vector<int>> matches_idx;
        std::vector<int> unmatch_track_idx;
        std::vector<int> unmatch_detection_idx;

        auto dists = CalcIouDistance(remain_tracked_stracks, det_low_stracks);
        LinearAssignment(dists, remain_tracked_stracks.size(), det_low_stracks.size(), 0.5,
                         matches_idx, unmatch_track_idx, unmatch_detection_idx);

        for (const auto& match_idx : matches_idx) {
            const auto& track = remain_tracked_stracks[match_idx[0]];
            const auto& det = det_low_stracks[match_idx[1]];
            if (track->GetState() == STrackState::kTracked) {
                track->Update(*det, frame_id_);
                current_tracked_stracks.push_back(track);
            } else {
                track->ReActivate(*det, frame_id_);
                refind_stracks.push_back(track);
            }
        }

        for (const auto& unmatch_track : unmatch_track_idx) {
            const auto& track = remain_tracked_stracks[unmatch_track];
            if (track->GetState() != STrackState::kLost) {
                track->MarkAsLost();
                current_lost_stracks.push_back(track);
            }
        }
    }

    ////////////////// Step 4: Init new stracks //////////////////
    std::vector<STrackPtr> current_removed_stracks;

    {
        std::vector<int> unmatch_detection_idx;
        std::vector<int> unmatch_unconfirmed_idx;
        std::vector<std::vector<int>> matches_idx;

        // Deal with unconfirmed tracks, usually tracks with only one beginning frame
        auto dists = CalcIouDistance(non_active_stracks, remain_det_stracks);
        LinearAssignment(dists, non_active_stracks.size(), remain_det_stracks.size(), 0.7,
                         matches_idx, unmatch_unconfirmed_idx, unmatch_detection_idx);

        for (const auto& match_idx : matches_idx) {
            non_active_stracks[match_idx[0]]->Update(*remain_det_stracks[match_idx[1]], frame_id_);
            current_tracked_stracks.push_back(non_active_stracks[match_idx[0]]);
        }

        for (const auto& unmatch_idx : unmatch_unconfirmed_idx) {
            const auto& track = non_active_stracks[unmatch_idx];
            track->MarkAsRemoved();
            current_removed_stracks.push_back(track);
        }

        // Add new stracks
        for (const auto& unmatch_idx : unmatch_detection_idx) {
            const auto& track = remain_det_stracks[unmatch_idx];
            if (track->GetScore() < high_thresh_) {
                continue;
            }
            track_id_count_++;
            track->Activate(frame_id_, track_id_count_);
            current_tracked_stracks.push_back(track);
        }
    }

    ////////////////// Step 5: Update state //////////////////
    for (const auto& lost_strack : lost_stracks_) {
        if (frame_id_ - lost_strack->GetFrameId() > max_time_lost_) {
            lost_strack->MarkAsRemoved();
            current_removed_stracks.push_back(lost_strack);
        }
    }

    tracked_stracks_ = JointStracks(current_tracked_stracks, refind_stracks);
    lost_stracks_ =
        SubStracks(JointStracks(SubStracks(lost_stracks_, tracked_stracks_), current_lost_stracks),
                   removed_stracks_);
    removed_stracks_ = JointStracks(removed_stracks_, current_removed_stracks);

    std::vector<STrackPtr> tracked_stracks_out;
    std::vector<STrackPtr> lost_stracks_out;
    RemoveDuplicateStracks(tracked_stracks_, lost_stracks_, tracked_stracks_out, lost_stracks_out);
    tracked_stracks_ = tracked_stracks_out;
    lost_stracks_ = lost_stracks_out;

    std::vector<STrackPtr> output_stracks;
    for (const auto& track : tracked_stracks_) {
        if (track->IsActivated()) {
            output_stracks.push_back(track);
        }
    }

    return output_stracks;
}

std::vector<ByteTracker::STrackPtr> ByteTracker::JointStracks(
    const std::vector<STrackPtr>& first_track_list,
    const std::vector<STrackPtr>& second_track_list) {
    std::map<int, int> exists;
    std::vector<STrackPtr> result;
    for (const auto& track : first_track_list) {
        exists.emplace(track->GetTrackId(), 1);
        result.push_back(track);
    }
    for (const auto& track : second_track_list) {
        int track_id = track->GetTrackId();
        if (!exists[track_id] || exists.count(track_id) == 0) {
            exists[track_id] = 1;
            result.push_back(track);
        }
    }
    return result;
}

std::vector<ByteTracker::STrackPtr> ByteTracker::SubStracks(
    const std::vector<STrackPtr>& first_track_list,
    const std::vector<STrackPtr>& second_track_list) {
    std::map<int, STrackPtr> stracks;
    for (const auto& track : first_track_list) {
        stracks.emplace(track->GetTrackId(), track);
    }

    for (const auto& track : second_track_list) {
        int track_id = track->GetTrackId();
        if (stracks.count(track_id) != 0) {
            stracks.erase(track_id);
        }
    }

    std::vector<STrackPtr> result;
    for (const auto& [track_id, track] : stracks) {
        result.push_back(track);
    }

    return result;
}

void ByteTracker::RemoveDuplicateStracks(const std::vector<STrackPtr>& a_stracks,
                                         const std::vector<STrackPtr>& b_stracks,
                                         std::vector<STrackPtr>& a_result,
                                         std::vector<STrackPtr>& b_result) {
    auto ious = CalcIouDistance(a_stracks, b_stracks);

    std::vector<std::pair<size_t, size_t>> overlapping_combinations;
    for (size_t row_idx = 0; row_idx < ious.size(); row_idx++) {
        for (size_t col_idx = 0; col_idx < ious[row_idx].size(); col_idx++) {
            if (ious[row_idx][col_idx] < 0.15) {
                overlapping_combinations.emplace_back(row_idx, col_idx);
            }
        }
    }

    std::vector<bool> a_overlapping(a_stracks.size(), false);
    std::vector<bool> b_overlapping(b_stracks.size(), false);
    for (const auto& [a_idx, b_idx] : overlapping_combinations) {
        int a_age = a_stracks[a_idx]->GetFrameId() - a_stracks[a_idx]->GetStartFrameId();
        int b_age = b_stracks[b_idx]->GetFrameId() - b_stracks[b_idx]->GetStartFrameId();
        if (a_age > b_age) {
            b_overlapping[b_idx] = true;
        } else {
            a_overlapping[a_idx] = true;
        }
    }

    for (size_t a_idx = 0; a_idx < a_stracks.size(); a_idx++) {
        if (!a_overlapping[a_idx]) {
            a_result.push_back(a_stracks[a_idx]);
        }
    }

    for (size_t b_idx = 0; b_idx < b_stracks.size(); b_idx++) {
        if (!b_overlapping[b_idx]) {
            b_result.push_back(b_stracks[b_idx]);
        }
    }
}

void ByteTracker::LinearAssignment(const std::vector<std::vector<float>>& cost_matrix, int num_rows,
                                   int num_cols, float thresh,
                                   std::vector<std::vector<int>>& matches,
                                   std::vector<int>& unmatched_rows,
                                   std::vector<int>& unmatched_cols) {
    if (cost_matrix.size() == 0) {
        for (int row_idx = 0; row_idx < num_rows; row_idx++) {
            unmatched_rows.push_back(row_idx);
        }
        for (int col_idx = 0; col_idx < num_cols; col_idx++) {
            unmatched_cols.push_back(col_idx);
        }
        return;
    }

    std::vector<int> rowsol;
    std::vector<int> colsol;
    ExecLapjv(cost_matrix, rowsol, colsol, true, thresh);
    for (size_t row_idx = 0; row_idx < rowsol.size(); row_idx++) {
        if (rowsol[row_idx] >= 0) {
            std::vector<int> match;
            match.push_back(row_idx);
            match.push_back(rowsol[row_idx]);
            matches.push_back(match);
        } else {
            unmatched_rows.push_back(row_idx);
        }
    }

    for (size_t col_idx = 0; col_idx < colsol.size(); col_idx++) {
        if (colsol[col_idx] < 0) {
            unmatched_cols.push_back(col_idx);
        }
    }
}

std::vector<std::vector<float>> ByteTracker::CalcIous(const std::vector<Rect>& a_rects,
                                                      const std::vector<Rect>& b_rects) {
    std::vector<std::vector<float>> ious;
    if (a_rects.size() * b_rects.size() == 0) {
        return ious;
    }

    ious.resize(a_rects.size());
    for (auto& iou_row : ious) {
        iou_row.resize(b_rects.size());
    }

    for (size_t b_idx = 0; b_idx < b_rects.size(); b_idx++) {
        for (size_t a_idx = 0; a_idx < a_rects.size(); a_idx++) {
            ious[a_idx][b_idx] = b_rects[b_idx].CalcIoU(a_rects[a_idx]);
        }
    }
    return ious;
}

std::vector<std::vector<float>> ByteTracker::CalcIouDistance(
    const std::vector<STrackPtr>& a_tracks, const std::vector<STrackPtr>& b_tracks) {
    std::vector<Rect> a_rects;
    std::vector<Rect> b_rects;
    for (const auto& track : a_tracks) {
        a_rects.push_back(track->GetRect());
    }
    for (const auto& track : b_tracks) {
        b_rects.push_back(track->GetRect());
    }

    auto ious = CalcIous(a_rects, b_rects);

    std::vector<std::vector<float>> cost_matrix;
    for (auto& iou_row : ious) {
        std::vector<float> cost_row;
        for (float iou : iou_row) {
            cost_row.push_back(1 - iou);
        }
        cost_matrix.push_back(cost_row);
    }

    return cost_matrix;
}

// NOLINTNEXTLINE(readability-function-cognitive-complexity) — faithful port of
// the upstream cost-matrix extension glue around the vendored LAPJV solver.
double ByteTracker::ExecLapjv(const std::vector<std::vector<float>>& cost, std::vector<int>& rowsol,
                              std::vector<int>& colsol, bool extend_cost, float cost_limit,
                              bool return_cost) {
    std::vector<std::vector<float>> cost_c;
    cost_c.assign(cost.begin(), cost.end());

    std::vector<std::vector<float>> cost_c_extended;

    int n_rows = cost.size();
    int n_cols = cost[0].size();
    rowsol.resize(n_rows);
    colsol.resize(n_cols);

    int matrix_size = 0;
    if (n_rows == n_cols) {
        matrix_size = n_rows;
    } else if (!extend_cost) {
        throw std::runtime_error("The `extend_cost` variable should set True");
    }

    if (extend_cost || cost_limit < std::numeric_limits<float>::max()) {
        matrix_size = n_rows + n_cols;
        cost_c_extended.resize(matrix_size);
        for (auto& row : cost_c_extended) {
            row.resize(matrix_size);
        }

        if (cost_limit < std::numeric_limits<float>::max()) {
            for (auto& row : cost_c_extended) {
                for (auto& value : row) {
                    value = cost_limit / 2.0;
                }
            }
        } else {
            float cost_max = -1;
            for (const auto& row : cost_c) {
                for (float value : row) {
                    if (value > cost_max) {
                        cost_max = value;
                    }
                }
            }
            for (auto& row : cost_c_extended) {
                for (auto& value : row) {
                    value = cost_max + 1;
                }
            }
        }

        for (size_t row_idx = n_rows; row_idx < cost_c_extended.size(); row_idx++) {
            for (size_t col_idx = n_cols; col_idx < cost_c_extended[row_idx].size(); col_idx++) {
                cost_c_extended[row_idx][col_idx] = 0;
            }
        }
        for (int row_idx = 0; row_idx < n_rows; row_idx++) {
            for (int col_idx = 0; col_idx < n_cols; col_idx++) {
                cost_c_extended[row_idx][col_idx] = cost_c[row_idx][col_idx];
            }
        }

        cost_c.clear();
        cost_c.assign(cost_c_extended.begin(), cost_c_extended.end());
    }

    auto** cost_ptr = new double*[sizeof(double*) * matrix_size];
    for (int row_idx = 0; row_idx < matrix_size; row_idx++) {
        cost_ptr[row_idx] = new double[sizeof(double) * matrix_size];
    }

    for (int row_idx = 0; row_idx < matrix_size; row_idx++) {
        for (int col_idx = 0; col_idx < matrix_size; col_idx++) {
            cost_ptr[row_idx][col_idx] = cost_c[row_idx][col_idx];
        }
    }

    int* x_c = new int[sizeof(int) * matrix_size];
    int* y_c = new int[sizeof(int) * matrix_size];

    int ret = lapjv_internal(matrix_size, cost_ptr, x_c, y_c);
    if (ret != 0) {
        throw std::runtime_error("The result of lapjv_internal() is invalid.");
    }

    double opt = 0.0;

    if (matrix_size != n_rows) {
        for (int idx = 0; idx < matrix_size; idx++) {
            if (x_c[idx] >= n_cols) {
                x_c[idx] = -1;
            }
            if (y_c[idx] >= n_rows) {
                y_c[idx] = -1;
            }
        }
        for (int row_idx = 0; row_idx < n_rows; row_idx++) {
            rowsol[row_idx] = x_c[row_idx];
        }
        for (int col_idx = 0; col_idx < n_cols; col_idx++) {
            colsol[col_idx] = y_c[col_idx];
        }

        if (return_cost) {
            for (size_t row_idx = 0; row_idx < rowsol.size(); row_idx++) {
                if (rowsol[row_idx] != -1) {
                    opt += cost_ptr[row_idx][rowsol[row_idx]];
                }
            }
        }
    } else if (return_cost) {
        for (size_t row_idx = 0; row_idx < rowsol.size(); row_idx++) {
            opt += cost_ptr[row_idx][rowsol[row_idx]];
        }
    }

    for (int row_idx = 0; row_idx < matrix_size; row_idx++) {
        delete[] cost_ptr[row_idx];
    }
    delete[] cost_ptr;
    delete[] x_c;
    delete[] y_c;

    return opt;
}

}  // namespace byte_track
