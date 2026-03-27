/**
 * =============================================================================
 *  imu_fusion.h — Sensor Fusion Module (Kalman Filters for Pitch/Roll, Gyro Yaw)
 *  Board: WHEELTEC STM32F407VET6
 * =============================================================================
 *
 *  OVERVIEW:
 *    This module owns the ICM-20948 sensor data and Kalman filters.
 *    - Pitch and Roll: Kalman filtered (accel + gyro fusion)
 *    - Yaw: Gyro integration only (no magnetometer)
 *
 *    Yaw will drift over time. Call IMU_Fusion_ResetYaw() after each
 *    maneuver to prevent drift accumulation.
 *
 *  USAGE:
 *    1. IMU_Fusion_Init(&hi2c2)         — once at startup
 *    2. IMU_Fusion_SetGyroBias(500)     — once at startup (robot stationary)
 *    3. IMU_Fusion_Update()             — every 10 ms in main loop
 *    4. IMU_Fusion_GetYaw()             — get current yaw angle
 *    5. IMU_Fusion_ResetYaw()           — reset yaw to 0 after each turn
 *
 *  DEPENDENCIES:
 *    icm20948.h, kalman.h
 *
 * =============================================================================
 */

#ifndef IMU_FUSION_H
#define IMU_FUSION_H

#include "main.h"
#include "icm20948.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ===========================================================================
 *  PUBLIC API — Initialization
 * =========================================================================== */

/**
 * @brief  Initialize ICM-20948 and Kalman filters.
 *         Must be called once after I2C is initialized.
 * @param  hi2c  Pointer to I2C handle (e.g., &hi2c2)
 * @return true on success, false if IMU communication failed
 */
bool IMU_Fusion_Init(I2C_HandleTypeDef *hi2c);

/**
 * @brief  Calibrate gyro Z bias. Call with robot STATIONARY.
 *         Averages readings to find the zero-point offset.
 * @param  samples  Number of samples to average (recommend 500)
 */
void IMU_Fusion_SetGyroBias(uint16_t samples);

/* ===========================================================================
 *  PUBLIC API — Update (call every 10 ms)
 * =========================================================================== */

/**
 * @brief  Run one fusion cycle: read sensors, update filters, integrate yaw.
 *         Call this every 10 ms in your main loop or timer ISR.
 */
void IMU_Fusion_Update(void);

/* ===========================================================================
 *  PUBLIC API — Getters
 * =========================================================================== */

/**
 * @brief  Get current yaw angle (gyro-integrated).
 * @return Yaw in degrees (-180 to +180), positive = clockwise
 */
float IMU_Fusion_GetYaw(void);

/**
 * @brief  Get Kalman-filtered pitch angle.
 * @return Pitch in degrees, positive = nose up
 */
float IMU_Fusion_GetPitch(void);

/**
 * @brief  Get Kalman-filtered roll angle.
 * @return Roll in degrees, positive = right side down
 */
float IMU_Fusion_GetRoll(void);

/**
 * @brief  Get latest gyro Z rate (bias-corrected).
 * @return Angular rate in degrees/second
 */
float IMU_Fusion_GetGyroZ(void);

/**
 * @brief  Get pointer to latest raw sensor data.
 * @return Pointer to ICM20948_Data_t structure
 */
const ICM20948_Data_t* IMU_Fusion_GetRawData(void);

/**
 * @brief  Get the current gyro Z bias value (for debugging).
 * @return Bias in degrees/second
 */
float IMU_Fusion_GetGyroBias(void);

/* ===========================================================================
 *  PUBLIC API — Yaw Control
 * =========================================================================== */

/**
 * @brief  Reset yaw to zero.
 *         Call this after completing a maneuver to prevent drift accumulation.
 */
void IMU_Fusion_ResetYaw(void);

/**
 * @brief  Set yaw to a specific value.
 * @param  yaw  Desired yaw angle in degrees
 */
void IMU_Fusion_SetYaw(float yaw);

#ifdef __cplusplus
}
#endif

#endif /* IMU_FUSION_H */