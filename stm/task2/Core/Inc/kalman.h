/**
 * =============================================================================
 *  kalman.h — 1D Kalman Filter for IMU Angle Estimation
 *  Board: WHEELTEC STM32F407VET6
 * =============================================================================
 *
 *  This implements a simple 2-state Kalman filter that fuses:
 *    - Gyroscope data  (fast, smooth, but drifts over time)
 *    - A reference measurement (noisy, but no drift)
 *
 *  The filter estimates two things:
 *    1. The angle (in degrees)
 *    2. The gyroscope bias (how much the gyro drifts)
 *
 *  THREE instances are used for full orientation:
 *    - Pitch:  gyro Y + accelerometer reference  (atan2 of accel X, Z)
 *    - Roll:   gyro X + accelerometer reference  (atan2 of accel Y, Z)
 *    - Yaw:    gyro Z + magnetometer reference   (atan2 of mag Y, X)
 *
 * =============================================================================
 */

#ifndef KALMAN_H
#define KALMAN_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    /* Estimated state */
    float angle;
    float bias;

    /* Error covariance matrix (2x2) */
    float P[2][2];

    /* Tuning parameters */
    float Q_angle;
    float Q_bias;
    float R_measure;
} Kalman_t;

/**
 * @brief  Initialise a Kalman filter instance with default tuning.
 *
 * Default tuning:
 *   Q_angle   = 0.001
 *   Q_bias    = 0.003
 *   R_measure = 0.03
 */
void Kalman_Init(Kalman_t *kf);

/**
 * @brief  Set the initial angle estimate.
 */
void Kalman_SetAngle(Kalman_t *kf, float angle);

/**
 * @brief  Run one filter iteration.
 * @param  ref_angle   Reference angle measurement (degrees)
 *                     For pitch/roll: from accelerometer
 *                     For yaw: from magnetometer heading
 * @param  gyro_rate   Angular rate from gyroscope (degrees/second)
 * @param  dt          Time step in seconds
 * @return Filtered angle estimate (degrees)
 */
float Kalman_Update(Kalman_t *kf, float ref_angle, float gyro_rate, float dt);

#ifdef __cplusplus
}
#endif

#endif /* KALMAN_H */