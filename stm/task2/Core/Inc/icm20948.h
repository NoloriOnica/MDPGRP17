/**
 * =============================================================================
 *  icm20948.h — ICM-20948 IMU Driver (Accelerometer + Gyroscope only)
 *  Board: WHEELTEC STM32F407VET6 (Rev 23.0)
 * =============================================================================
 *
 *  This is a simplified driver that only uses accelerometer and gyroscope.
 *  Magnetometer code has been removed for simplicity.
 *
 *  Wiring (from schematic Sheet 2):
 *    ICM-20948 (U18) ←→ Level Shifter (U19, RS0102YH8) ←→ STM32
 *      SCL/SCLK  (pin 23) → B1 → A1 → PB10 (I2C2_SCL)
 *      SDA/SDI   (pin 24) → B2 → A2 → PB11 (I2C2_SDA)
 *      SDO/AD0   (pin 9)  → GND via R61  →  I2C address = 0x68
 *
 *  CubeMX Configuration:
 *    I2C2: Fast Mode (400 kHz), PB10=SCL, PB11=SDA
 *
 * =============================================================================
 */

#ifndef ICM20948_H
#define ICM20948_H

#include "main.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ===========================================================================
 *  I2C ADDRESS
 * =========================================================================== */
#define ICM20948_I2C_ADDR       (0x68 << 1)   /* = 0xD0 */

/* ===========================================================================
 *  REGISTER BANK SELECT
 * =========================================================================== */
#define REG_BANK_SEL            0x7F

/* ===========================================================================
 *  BANK 0 REGISTERS
 * =========================================================================== */
#define B0_WHO_AM_I             0x00
#define B0_USER_CTRL            0x03
#define B0_LP_CONFIG            0x05
#define B0_PWR_MGMT_1           0x06
#define B0_PWR_MGMT_2           0x07
#define B0_INT_PIN_CFG          0x0F
#define B0_INT_ENABLE           0x10
#define B0_INT_ENABLE_1         0x11
#define B0_INT_STATUS           0x19
#define B0_INT_STATUS_1         0x1A

/* Accelerometer output */
#define B0_ACCEL_XOUT_H         0x2D
#define B0_ACCEL_XOUT_L         0x2E
#define B0_ACCEL_YOUT_H         0x2F
#define B0_ACCEL_YOUT_L         0x30
#define B0_ACCEL_ZOUT_H         0x31
#define B0_ACCEL_ZOUT_L         0x32

/* Gyroscope output */
#define B0_GYRO_XOUT_H          0x33
#define B0_GYRO_XOUT_L          0x34
#define B0_GYRO_YOUT_H          0x35
#define B0_GYRO_YOUT_L          0x36
#define B0_GYRO_ZOUT_H          0x37
#define B0_GYRO_ZOUT_L          0x38

/* Temperature output */
#define B0_TEMP_OUT_H           0x39
#define B0_TEMP_OUT_L           0x3A

/* ===========================================================================
 *  BANK 2 REGISTERS — Sensor configuration
 * =========================================================================== */
#define B2_GYRO_SMPLRT_DIV      0x00
#define B2_GYRO_CONFIG_1        0x01
#define B2_ACCEL_SMPLRT_DIV_1   0x10
#define B2_ACCEL_SMPLRT_DIV_2   0x11
#define B2_ACCEL_CONFIG         0x14

/* ===========================================================================
 *  CONFIGURATION VALUES
 * =========================================================================== */
#define ICM20948_WHO_AM_I_VAL   0xEA

#define PWR_MGMT_1_RESET        0x80
#define PWR_MGMT_1_SLEEP        0x40
#define PWR_MGMT_1_CLK_AUTO     0x01

/* Gyroscope full-scale range */
typedef enum {
    GYRO_FS_250DPS  = 0x00,
    GYRO_FS_500DPS  = 0x02,
    GYRO_FS_1000DPS = 0x04,
    GYRO_FS_2000DPS = 0x06,
} ICM20948_GyroFS_t;

/* Accelerometer full-scale range */
typedef enum {
    ACCEL_FS_2G     = 0x00,
    ACCEL_FS_4G     = 0x02,
    ACCEL_FS_8G     = 0x04,
    ACCEL_FS_16G    = 0x06,
} ICM20948_AccelFS_t;

/* ===========================================================================
 *  SENSITIVITY SCALE FACTORS
 * =========================================================================== */
#define GYRO_SENSITIVITY_250DPS     131.0f
#define GYRO_SENSITIVITY_500DPS      65.5f
#define GYRO_SENSITIVITY_1000DPS     32.8f
#define GYRO_SENSITIVITY_2000DPS     16.4f

#define ACCEL_SENSITIVITY_2G     16384.0f
#define ACCEL_SENSITIVITY_4G      8192.0f
#define ACCEL_SENSITIVITY_8G      4096.0f
#define ACCEL_SENSITIVITY_16G     2048.0f

#define TEMP_SENSITIVITY        333.87f
#define TEMP_OFFSET              21.0f

/* ===========================================================================
 *  DATA STRUCTURES
 * =========================================================================== */

typedef struct {
    int16_t x;
    int16_t y;
    int16_t z;
} ICM20948_RawData_t;

typedef struct {
    float x;
    float y;
    float z;
} ICM20948_ScaledData_t;

/* Complete sensor state (no magnetometer) */
typedef struct {
    ICM20948_RawData_t    accel_raw;
    ICM20948_RawData_t    gyro_raw;
    int16_t               temp_raw;

    ICM20948_ScaledData_t accel_g;       /* Acceleration in g              */
    ICM20948_ScaledData_t gyro_dps;      /* Angular rate in deg/sec        */
    float                 temp_c;        /* Temperature in Celsius         */
} ICM20948_Data_t;

/* ===========================================================================
 *  PUBLIC API
 * =========================================================================== */

/**
 * @brief  Initialize the ICM-20948 sensor.
 * @param  hi2c      Pointer to I2C handle
 * @param  gyro_fs   Gyroscope full-scale range
 * @param  accel_fs  Accelerometer full-scale range
 * @return HAL_OK on success, HAL_ERROR on failure
 */
HAL_StatusTypeDef ICM20948_Init(I2C_HandleTypeDef *hi2c,
                                ICM20948_GyroFS_t gyro_fs,
                                ICM20948_AccelFS_t accel_fs);

/**
 * @brief  Read all sensor data (accel + gyro + temp).
 * @param  hi2c  Pointer to I2C handle
 * @param  data  Pointer to data structure to fill
 * @return HAL_OK on success
 */
HAL_StatusTypeDef ICM20948_ReadAll(I2C_HandleTypeDef *hi2c,
                                   ICM20948_Data_t *data);

/**
 * @brief  Read accelerometer only.
 * @param  hi2c  Pointer to I2C handle
 * @param  data  Pointer to data structure to fill
 * @return HAL_OK on success
 */
HAL_StatusTypeDef ICM20948_ReadAccel(I2C_HandleTypeDef *hi2c,
                                     ICM20948_Data_t *data);

/**
 * @brief  Read gyroscope only.
 * @param  hi2c  Pointer to I2C handle
 * @param  data  Pointer to data structure to fill
 * @return HAL_OK on success
 */
HAL_StatusTypeDef ICM20948_ReadGyro(I2C_HandleTypeDef *hi2c,
                                    ICM20948_Data_t *data);

/**
 * @brief  Read WHO_AM_I register to verify sensor is present.
 * @param  hi2c  Pointer to I2C handle
 * @return WHO_AM_I value (should be 0xEA)
 */
uint8_t ICM20948_WhoAmI(I2C_HandleTypeDef *hi2c);

#ifdef __cplusplus
}
#endif

#endif /* ICM20948_H */