// Adapted from ByteTrack-cpp (https://github.com/Vertical-Beach/ByteTrack-cpp),
// MIT License. See third_party/ByteTrack-cpp.LICENSE. Reformatted to this
// repo's Google C++ style; tracking behaviour is unchanged.

#include "bytetrack/kalman_filter.hpp"

#include <cstddef>

namespace byte_track {

KalmanFilter::KalmanFilter(float std_weight_position, float std_weight_velocity)
    : std_weight_position_(std_weight_position), std_weight_velocity_(std_weight_velocity) {
    constexpr size_t kMeasurementDims = 4;
    constexpr float kTimeStep = 1;

    motion_mat_ = Eigen::MatrixXf::Identity(8, 8);
    update_mat_ = Eigen::MatrixXf::Identity(4, 8);

    for (size_t dimension_idx = 0; dimension_idx < kMeasurementDims; dimension_idx++) {
        motion_mat_(dimension_idx, kMeasurementDims + dimension_idx) = kTimeStep;
    }
}

void KalmanFilter::Initiate(StateMean& mean, StateCov& covariance, const DetectBox& measurement) {
    mean.block<1, 4>(0, 0) = measurement.block<1, 4>(0, 0);
    mean.block<1, 4>(0, 4) = Eigen::Vector4f::Zero();

    StateMean std_dev;
    std_dev(0) = 2 * std_weight_position_ * measurement[3];
    std_dev(1) = 2 * std_weight_position_ * measurement[3];
    std_dev(2) = 1e-2;
    std_dev(3) = 2 * std_weight_position_ * measurement[3];
    std_dev(4) = 10 * std_weight_velocity_ * measurement[3];
    std_dev(5) = 10 * std_weight_velocity_ * measurement[3];
    std_dev(6) = 1e-5;
    std_dev(7) = 10 * std_weight_velocity_ * measurement[3];

    StateMean variance = std_dev.array().square();
    covariance = variance.asDiagonal();
}

void KalmanFilter::Predict(StateMean& mean, StateCov& covariance) {
    StateMean std_dev;
    std_dev(0) = std_weight_position_ * mean(3);
    std_dev(1) = std_weight_position_ * mean(3);
    std_dev(2) = 1e-2;
    std_dev(3) = std_weight_position_ * mean(3);
    std_dev(4) = std_weight_velocity_ * mean(3);
    std_dev(5) = std_weight_velocity_ * mean(3);
    std_dev(6) = 1e-5;
    std_dev(7) = std_weight_velocity_ * mean(3);

    StateMean variance = std_dev.array().square();
    StateCov motion_cov = variance.asDiagonal();

    mean = motion_mat_ * mean.transpose();
    covariance = motion_mat_ * covariance * (motion_mat_.transpose()) + motion_cov;
}

void KalmanFilter::Update(StateMean& mean, StateCov& covariance, const DetectBox& measurement) {
    StateHMean projected_mean;
    StateHCov projected_cov;
    Project(projected_mean, projected_cov, mean, covariance);

    Eigen::Matrix<float, 4, 8> measurement_cov_factor =
        (covariance * (update_mat_.transpose())).transpose();
    Eigen::Matrix<float, 8, 4> kalman_gain =
        (projected_cov.llt().solve(measurement_cov_factor)).transpose();
    Eigen::Matrix<float, 1, 4> innovation = measurement - projected_mean;

    auto correction = innovation * (kalman_gain.transpose());
    mean = (mean.array() + correction.array()).matrix();
    covariance = covariance - kalman_gain * projected_cov * (kalman_gain.transpose());
}

void KalmanFilter::Project(StateHMean& projected_mean, StateHCov& projected_covariance,
                           const StateMean& mean, const StateCov& covariance) {
    DetectBox std_dev;
    std_dev << std_weight_position_ * mean(3), std_weight_position_ * mean(3), 1e-1,
        std_weight_position_ * mean(3);

    projected_mean = update_mat_ * mean.transpose();
    projected_covariance = update_mat_ * covariance * (update_mat_.transpose());

    Eigen::Matrix<float, 4, 4> measurement_noise = std_dev.asDiagonal();
    projected_covariance += measurement_noise.array().square().matrix();
}

}  // namespace byte_track
