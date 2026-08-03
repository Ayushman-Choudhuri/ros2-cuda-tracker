#pragma once

#include <Eigen/Dense>

namespace byte_track {

    // Constant-velocity Kalman filter over an 8-dimensional state:
    //   (center_x, center_y, aspect_ratio, height, and the velocity of each).
    // Measurements are the first four components of that state.
    class KalmanFilter {
       public:
        static constexpr int kStateDims = 8;
        static constexpr int kMeasurementDims = 4;

        enum StateIndex {
            kCenterX = 0,
            kCenterY = 1,
            kAspectRatio = 2,
            kHeight = 3,
            kCenterXVelocity = 4,
            kCenterYVelocity = 5,
            kAspectRatioVelocity = 6,
            kHeightVelocity = 7,
        };

        using DetectBox = Eigen::Matrix<float, 1, kMeasurementDims, Eigen::RowMajor>;
        using StateMean = Eigen::Matrix<float, 1, kStateDims, Eigen::RowMajor>;
        using StateCov = Eigen::Matrix<float, kStateDims, kStateDims, Eigen::RowMajor>;
        using MeasurementMean = Eigen::Matrix<float, 1, kMeasurementDims, Eigen::RowMajor>;
        using MeasurementCov =
            Eigen::Matrix<float, kMeasurementDims, kMeasurementDims, Eigen::RowMajor>;

        explicit KalmanFilter(float std_weight_position = 1.0F / 20,
                              float std_weight_velocity = 1.0F / 160);

        void Initiate(StateMean& mean, StateCov& covariance, const DetectBox& measurement) const;
        void Predict(StateMean& mean, StateCov& covariance) const;
        void Update(StateMean& mean, StateCov& covariance, const DetectBox& measurement) const;

       private:
        // Diagonal standard deviations of the process noise. Position and velocity
        // terms scale with box height; the aspect ratio terms are held constant
        // because a ratio does not grow with box size.
        [[nodiscard]] static StateMean ProcessNoiseStdDev(float position_std_dev,
                                                          float velocity_std_dev);

        void Project(MeasurementMean& projected_mean, MeasurementCov& projected_covariance,
                     const StateMean& mean, const StateCov& covariance) const;

        float std_weight_position_;
        float std_weight_velocity_;

        Eigen::Matrix<float, kStateDims, kStateDims, Eigen::RowMajor> motion_mat_;
        Eigen::Matrix<float, kMeasurementDims, kStateDims, Eigen::RowMajor> update_mat_;
    };

}  // namespace byte_track
