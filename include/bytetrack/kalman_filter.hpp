#pragma once

// Adapted from ByteTrack-cpp (https://github.com/Vertical-Beach/ByteTrack-cpp),
// MIT License. See third_party/ByteTrack-cpp.LICENSE. Reformatted to this
// repo's Google C++ style; tracking behaviour is unchanged.

#include <Eigen/Dense>

namespace byte_track {

// Constant-velocity Kalman filter tracking the (centre_x, centre_y,
// aspect_ratio, height) box state plus their velocities.
class KalmanFilter {
   public:
    using DetectBox = Eigen::Matrix<float, 1, 4, Eigen::RowMajor>;

    using StateMean = Eigen::Matrix<float, 1, 8, Eigen::RowMajor>;
    using StateCov = Eigen::Matrix<float, 8, 8, Eigen::RowMajor>;

    using StateHMean = Eigen::Matrix<float, 1, 4, Eigen::RowMajor>;
    using StateHCov = Eigen::Matrix<float, 4, 4, Eigen::RowMajor>;

    KalmanFilter(float std_weight_position = 1.0F / 20, float std_weight_velocity = 1.0F / 160);

    void Initiate(StateMean& mean, StateCov& covariance, const DetectBox& measurement);

    void Predict(StateMean& mean, StateCov& covariance);

    void Update(StateMean& mean, StateCov& covariance, const DetectBox& measurement);

   private:
    float std_weight_position_;
    float std_weight_velocity_;

    Eigen::Matrix<float, 8, 8, Eigen::RowMajor> motion_mat_;
    Eigen::Matrix<float, 4, 8, Eigen::RowMajor> update_mat_;

    void Project(StateHMean& projected_mean, StateHCov& projected_covariance, const StateMean& mean,
                 const StateCov& covariance);
};

}  // namespace byte_track
