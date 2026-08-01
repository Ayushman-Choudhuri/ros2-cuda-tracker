#include "bytetrack/kalman_filter.hpp"

namespace byte_track {
    namespace {

        constexpr float kAspectRatioPositionStdDev = 1e-2F;
        constexpr float kAspectRatioVelocityStdDev = 1e-5F;
        constexpr float kAspectRatioMeasurementStdDev = 1e-1F;

        // A newly initiated track has no velocity estimate yet, so its state is far
        // less certain than after the first update.
        constexpr float kInitialPositionStdDevScale = 2.0F;
        constexpr float kInitialVelocityStdDevScale = 10.0F;

    }  // namespace

    KalmanFilter::KalmanFilter(float std_weight_position, float std_weight_velocity)
        : std_weight_position_(std_weight_position),
          std_weight_velocity_(std_weight_velocity),
          motion_mat_(Eigen::MatrixXf::Identity(kStateDims, kStateDims)),
          update_mat_(Eigen::MatrixXf::Identity(kMeasurementDims, kStateDims)) {
        constexpr float kTimeStep = 1.0F;
        for (int dimension = 0; dimension < kMeasurementDims; ++dimension) {
            motion_mat_(dimension, kMeasurementDims + dimension) = kTimeStep;
        }
    }

    KalmanFilter::StateMean KalmanFilter::ProcessNoiseStdDev(float position_std_dev,
                                                             float velocity_std_dev) {
        StateMean std_dev;
        std_dev << position_std_dev, position_std_dev, kAspectRatioPositionStdDev, position_std_dev,
            velocity_std_dev, velocity_std_dev, kAspectRatioVelocityStdDev, velocity_std_dev;
        return std_dev;
    }

    void KalmanFilter::Initiate(StateMean& mean, StateCov& covariance,
                                const DetectBox& measurement) const {
        mean.block<1, kMeasurementDims>(0, 0) = measurement.block<1, kMeasurementDims>(0, 0);
        mean.block<1, kMeasurementDims>(0, kMeasurementDims) = Eigen::Vector4f::Zero();

        const float height = measurement[kHeight];
        const StateMean std_dev =
            ProcessNoiseStdDev(kInitialPositionStdDevScale * std_weight_position_ * height,
                               kInitialVelocityStdDevScale * std_weight_velocity_ * height);
        covariance = StateMean(std_dev.array().square()).asDiagonal();
    }

    void KalmanFilter::Predict(StateMean& mean, StateCov& covariance) const {
        const float height = mean(kHeight);
        const StateMean std_dev =
            ProcessNoiseStdDev(std_weight_position_ * height, std_weight_velocity_ * height);
        const StateCov motion_cov = StateMean(std_dev.array().square()).asDiagonal();

        mean = motion_mat_ * mean.transpose();
        covariance = motion_mat_ * covariance * motion_mat_.transpose() + motion_cov;
    }

    void KalmanFilter::Update(StateMean& mean, StateCov& covariance,
                              const DetectBox& measurement) const {
        MeasurementMean projected_mean;
        MeasurementCov projected_covariance;
        Project(projected_mean, projected_covariance, mean, covariance);

        const Eigen::Matrix<float, kMeasurementDims, kStateDims> measurement_cov_factor =
            (covariance * update_mat_.transpose()).transpose();
        const Eigen::Matrix<float, kStateDims, kMeasurementDims> kalman_gain =
            projected_covariance.llt().solve(measurement_cov_factor).transpose();
        const MeasurementMean innovation = measurement - projected_mean;

        mean = (mean.array() + (innovation * kalman_gain.transpose()).array()).matrix();
        covariance = covariance - kalman_gain * projected_covariance * kalman_gain.transpose();
    }

    void KalmanFilter::Project(MeasurementMean& projected_mean,
                               MeasurementCov& projected_covariance, const StateMean& mean,
                               const StateCov& covariance) const {
        const float position_std_dev = std_weight_position_ * mean(kHeight);
        DetectBox std_dev;
        std_dev << position_std_dev, position_std_dev, kAspectRatioMeasurementStdDev,
            position_std_dev;

        projected_mean = update_mat_ * mean.transpose();
        projected_covariance = update_mat_ * covariance * update_mat_.transpose();

        const MeasurementCov measurement_noise = std_dev.asDiagonal();
        projected_covariance += measurement_noise.array().square().matrix();
    }

}  // namespace byte_track
