#include "ukf.h"
#include "Eigen/Dense"

using Eigen::MatrixXd;
using Eigen::VectorXd;

/**
 * Initializes Unscented Kalman filter
 */
UKF::UKF()
{
    // if this is false, laser measurements will be ignored (except during init)
    use_laser_ = true;

    // if this is false, radar measurements will be ignored (except during init)
    use_radar_ = true;

    // initial state vector
    x_ = VectorXd(5);

    // initial covariance matrix
    P_ = MatrixXd(5, 5);

    // Process noise standard deviation longitudinal acceleration in m/s^2
    std_a_ = 2.0;

    // Process noise standard deviation yaw acceleration in rad/s^2
    std_yawdd_ = 1.0;

    /**
   * DO NOT MODIFY measurement noise values below.
   * These are provided by the sensor manufacturer.
   */

    // Laser measurement noise standard deviation position1 in m
    std_laspx_ = 0.15;

    // Laser measurement noise standard deviation position2 in m
    std_laspy_ = 0.15;

    // Radar measurement noise standard deviation radius in m
    std_radr_ = 0.3;

    // Radar measurement noise standard deviation angle in rad
    std_radphi_ = 0.03;

    // Radar measurement noise standard deviation radius change in m/s
    std_radrd_ = 0.3;

    /**
   * End DO NOT MODIFY section for measurement noise values
   */

    /**
   * TODO: Complete the initialization. See ukf.h for other member properties.
   * Hint: one or more values initialized above might be wildly off...
   */
    is_initialized_ = false;
    n_x_ = 5;
    n_aug_ = 7;
    lambda_ = 0;
    Xsig_pred_ = MatrixXd(n_x_, 2 * n_aug_ + 1);
    weights_ = VectorXd(2*n_aug_+1);
    time_us_ = 0;
}

UKF::~UKF() {}

void UKF::ProcessMeasurement(MeasurementPackage meas_package)
{
    /**
   * TODO: Complete this function! Make sure you switch between lidar and radar
   * measurements.
   */
    if (!is_initialized_)
    {
        if (meas_package.sensor_type_ == MeasurementPackage::RADAR)
        {
            double rho = meas_package.raw_measurements_(0);
            double phi = meas_package.raw_measurements_(1);
            double rhodot = meas_package.raw_measurements_(2);
            double x = rho * cos(phi);
            double y = rho * sin(phi);
            double vx = rhodot * cos(phi);
            double vy = rhodot * sin(phi);
            double v = sqrt(vx * vx + vy * vy);
            x_ << x, y, v, rho, rhodot;
            P_ << std_radr_*std_radr_, 0, 0, 0, 0,
                    0, std_radr_*std_radr_, 0, 0, 0,
                    0, 0, std_radrd_*std_radrd_, 0, 0,
                    0, 0, 0, std_radphi_, 0,
                    0, 0, 0, 0, std_radphi_;
        }
        if (meas_package.sensor_type_ == MeasurementPackage::LASER)
        {
            // Initialize state.
            x_ << meas_package.raw_measurements_(0), meas_package.raw_measurements_(1), 0, 0, 0.0;
            P_ << std_laspx_*std_laspx_, 0, 0, 0, 0,
                    0, std_laspy_*std_laspy_, 0, 0, 0,
                    0, 0, 1, 0, 0,
                    0, 0, 0, 1, 0,
                    0, 0, 0, 0, 1;
        }
        is_initialized_ = true;
        time_us_ = meas_package.timestamp_;
    }
    double delta_t = (meas_package.timestamp_ - time_us_) / 1000000.0;

    time_us_ = meas_package.timestamp_;

    // Predict
    Prediction(delta_t);

    // Measurement updates
    if (meas_package.sensor_type_ == MeasurementPackage::RADAR)
    {
        UpdateRadar(meas_package);
    }
    if (meas_package.sensor_type_ == MeasurementPackage::LASER)
    {
        UpdateLidar(meas_package);
    }
}

void UKF::Prediction(double delta_t)
{
    /**
   * TODO: Complete this function! Estimate the object's location.
   * Modify the state vector, x_. Predict sigma points, the state,
   * and the state covariance matrix.
   */
    // Define spreading parameter
    lambda_ = 3 - n_x_;

    //create sigma point matrix
    MatrixXd Xsig_ = MatrixXd(n_x_, 2 * n_x_ + 1);

    //calculate square root of P
    MatrixXd A_ = P_.llt().matrixL();

    //calculate sigma points, set sigma points as columns of matrix Xsig_
    Xsig_.col(0) = x_;
    for(int i = 0; i < n_x_; i++)
    {
        Xsig_.col(i+1) = x_ + std::sqrt(lambda_+n_x_)*A_.col(i);
        Xsig_.col(i+1+n_x_) = x_ - std::sqrt(lambda_+n_x_)*A_.col(i);
    }

    // Define spreading parameter for augmentation
    lambda_ = 3 - n_aug_;

    //create matrices and vectors
    VectorXd x_aug_ = VectorXd(7);
    MatrixXd P_aug_ = MatrixXd(7, 7);
    MatrixXd Xsig_aug_ = MatrixXd(n_aug_, 2 * n_aug_ + 1);

    //create augmented mean state
    x_aug_.head(5) = x_;
    x_aug_(5) = 0;
    x_aug_(6) = 0;

    //create augmented covariance matrix
    MatrixXd Q = MatrixXd(2,2);
    Q << std_a_*std_a_, 0,
            0, std_yawdd_*std_yawdd_;
    P_aug_.fill(0.0);
    P_aug_.topLeftCorner(5, 5) = P_;
    P_aug_.bottomRightCorner(2, 2) = Q;

    //create square root matrix
    MatrixXd A_aug = P_aug_.llt().matrixL();

    //create augmented sigma points
    Xsig_aug_.col(0) = x_aug_;
    for(int i = 0; i < n_aug_; i++)
    {
        Xsig_aug_.col(i+1) = x_aug_ + std::sqrt(lambda_+n_aug_)*A_aug.col(i);
        Xsig_aug_.col(i+1+n_aug_) = x_aug_ - std::sqrt(lambda_+n_aug_)*A_aug.col(i);
    }

    //predict sigma points
    VectorXd vec1 = VectorXd(5);
    VectorXd vec2 = VectorXd(5);

    for(int i = 0; i < 2 * n_aug_ + 1; i++)
    {
        VectorXd calc_col = Xsig_aug_.col(i);
        double v = calc_col(2);
        double yaw = calc_col(3);
        double yawd = calc_col(4);
        double v_aug = calc_col(5);
        double v_yawdd = calc_col(6);

        //original
        VectorXd orig = calc_col.head(5);

        if(fabs(yawd) > .001)
        {
            // Avoid division by zero
            vec1 << (v/yawd)*(sin(yaw+yawd*delta_t) - sin(yaw)),
                    (v/yawd)*(-cos(yaw+yawd*delta_t) + cos(yaw)),
                    0,
                    yawd * delta_t,
                    0;
        }
        else
        {
            // If yaw rate is zero - alternative model
            vec1 << v*cos(yaw)*delta_t,
                    v*sin(yaw)*delta_t,
                    0,
                    yawd*delta_t,
                    0;
        }
        vec2 << .5*delta_t*delta_t*cos(yaw)*v_aug,
                .5*delta_t*delta_t*sin(yaw)*v_aug,
                delta_t*v_aug,
                .5*delta_t*delta_t*v_yawdd,
                delta_t*v_yawdd;

        Xsig_pred_.col(i) << orig + vec1 + vec2;
    }
    VectorXd x_pred = VectorXd(n_x_);
    MatrixXd P_pred = MatrixXd(n_x_, n_x_);
    x_pred.fill(0.0);
    P_pred.fill(0.0);
    for(int i = 0; i < 2 * n_aug_ + 1; i++)
    {
        if (i == 0)
        {
            weights_(i) = lambda_ / (lambda_ + n_aug_);
        } else
        {
            weights_(i) = .5 / (lambda_ + n_aug_);
        }
        x_pred += weights_(i) * Xsig_pred_.col(i);
    }
    for (int i = 0; i < 2 * n_aug_ + 1; i++)
    {
        //predict state covariance matrix
        VectorXd x_diff = Xsig_pred_.col(i) - x_pred;

        //normalize angles
        while (x_diff(3) > M_PI)
            x_diff(3) -= 2. * M_PI;
        while (x_diff(3) < -M_PI)
            x_diff(3) += 2. * M_PI;

        P_pred += weights_(i) * x_diff * x_diff.transpose();
    }
    x_ = x_pred;
    P_ = P_pred;
}

void UKF::UpdateLidar(MeasurementPackage meas_package)
{
    /**
   * TODO: Complete this function! Use lidar data to update the belief
   * about the object's position. Modify the state vector, x_, and
   * covariance, P_.
   * You can also calculate the lidar NIS, if desired.
   */

    //extract measurement as VectorXd
    VectorXd z = meas_package.raw_measurements_;
    //set measurement dimension, lidar can measure p_x and p_y
    int n_z = 2;
    //create matrix for sigma points in measurement space
    MatrixXd Zsig = MatrixXd(n_z, 2 * n_aug_ + 1);
    //transform sigma points into measurement space
    for (int i = 0; i < 2 * n_aug_ + 1; i++)
    { //2n+1 simga points
        // extract values for better readibility
        double p_x = Xsig_pred_(0, i);
        double p_y = Xsig_pred_(1, i);
        // measurement model
        Zsig(0, i) = p_x;
        Zsig(1, i) = p_y;
    }
    //mean predicted measurement
    VectorXd z_pred = VectorXd(n_z);
    z_pred.fill(0.0);
    for (int i = 0; i < 2 * n_aug_ + 1; i++)
    {
        z_pred = z_pred + weights_(i) * Zsig.col(i);
    }
    //measurement covariance matrix S
    MatrixXd S = MatrixXd(n_z, n_z);
    S.fill(0.0);
    for (int i = 0; i < 2 * n_aug_ + 1; i++)
    { //2n+1 simga points
        //residual
        VectorXd z_diff = Zsig.col(i) - z_pred;
        S = S + weights_(i) * z_diff * z_diff.transpose();
    }
    //add measurement noise covariance matrix
    MatrixXd R = MatrixXd(n_z, n_z);
    R << std_laspx_ * std_laspx_, 0,
            0, std_laspy_ * std_laspy_;
    S = S + R;
    //create matrix for cross correlation Tc
    MatrixXd Tc = MatrixXd(n_x_, n_z);
    /*****************************************************************************
  * UKF Update for Lidar
  ****************************************************************************/
    //calculate cross correlation matrix
    Tc.fill(0.0);
    for (int i = 0; i < 2 * n_aug_ + 1; i++)
    { //2n+1 simga points
        //residual
        VectorXd z_diff = Zsig.col(i) - z_pred;
        // state difference
        VectorXd x_diff = Xsig_pred_.col(i) - x_;
        Tc = Tc + weights_(i) * x_diff * z_diff.transpose();
    }
    //Kalman gain K;
    MatrixXd K = Tc * S.inverse();
    //residual
    VectorXd z_diff = z - z_pred;
    //update state mean and covariance matrix
    x_ = x_ + K * z_diff;
    P_ = P_ - K * S * K.transpose();
}


void UKF::UpdateRadar(MeasurementPackage meas_package) {
    /**
   * TODO: Complete this function! Use radar data to update the belief
   * about the object's position. Modify the state vector, x_, and
   * covariance, P_.
   * You can also calculate the radar NIS, if desired.
   */

    //extract measurement as VectorXd
    VectorXd z = meas_package.raw_measurements_;
    //set measurement dimension, radar can measure r, phi, and r_dot
    int n_z = 3;
    //create matrix for sigma points in measurement space
    MatrixXd Zsig = MatrixXd(n_z, 2 * n_aug_ + 1);
    //transform sigma points into measurement space
    for (int i = 0; i < 2 * n_aug_ + 1; i++)
    {
        // extract values for better readibility
        double p_x = Xsig_pred_(0, i);
        double p_y = Xsig_pred_(1, i);
        double v = Xsig_pred_(2, i);
        double yaw = Xsig_pred_(3, i);
        double v1 = cos(yaw)*v;
        double v2 = sin(yaw)*v;
        // measurement model
        Zsig(0, i) = sqrt(p_x*p_x + p_y*p_y); //r
        Zsig(1, i) = atan2(p_y, p_x); //phi
        Zsig(2, i) = (p_x*v1 + p_y*v2) / sqrt(p_x*p_x + p_y*p_y); //r_dot
    }
    //mean predicted measurement
    VectorXd z_pred = VectorXd(n_z);
    z_pred.fill(0.0);
    for (int i = 0; i < 2 * n_aug_ + 1; i++)
    {
        z_pred = z_pred + weights_(i) * Zsig.col(i);
    }
    //measurement covariance matrix S
    MatrixXd S = MatrixXd(n_z, n_z);
    S.fill(0.0);
    for (int i = 0; i < 2 * n_aug_ + 1; i++)
    {
        //residual
        VectorXd z_diff = Zsig.col(i) - z_pred;
        //angle normalization
        while (z_diff(1)> M_PI) z_diff(1) -= 2.*M_PI;
        while (z_diff(1)<-M_PI) z_diff(1) += 2.*M_PI;
        S = S + weights_(i) * z_diff * z_diff.transpose();
    }
    //add measurement noise covariance matrix
    MatrixXd R = MatrixXd(n_z, n_z);
    R << std_radr_*std_radr_, 0, 0,
            0, std_radphi_*std_radphi_, 0,
            0, 0, std_radrd_*std_radrd_;
    S = S + R;
    //create matrix for cross correlation Tc
    MatrixXd Tc = MatrixXd(n_x_, n_z);
    /*****************************************************************************
     * UKF Update for Radar
     ****************************************************************************/
    //calculate cross correlation matrix
    Tc.fill(0.0);
    for (int i = 0; i < 2 * n_aug_ + 1; i++)
    {
        //residual
        VectorXd z_diff = Zsig.col(i) - z_pred;
        //angle normalization
        while (z_diff(1)> M_PI) z_diff(1) -= 2.*M_PI;
        while (z_diff(1)<-M_PI) z_diff(1) += 2.*M_PI;
        // state difference
        VectorXd x_diff = Xsig_pred_.col(i) - x_;
        //angle normalization
        while (x_diff(3)> M_PI) x_diff(3) -= 2.*M_PI;
        while (x_diff(3)<-M_PI) x_diff(3) += 2.*M_PI;
        Tc = Tc + weights_(i) * x_diff * z_diff.transpose();
    }
    //Kalman gain K;
    MatrixXd K = Tc * S.inverse();
    //residual
    VectorXd z_diff = z - z_pred;
    //angle normalization
    while (z_diff(1)> M_PI) z_diff(1) -= 2.*M_PI;
    while (z_diff(1)<-M_PI) z_diff(1) += 2.*M_PI;
    //update state mean and covariance matrix
    x_ = x_ + K * z_diff;
    P_ = P_ - K*S*K.transpose();
}
