/**
 * =============================================================================
 *  imu_fusion.c — Sensor Fusion Module (Kalman Filters for Pitch/Roll, Gyro Yaw)
 *  Board: WHEELTEC STM32F407VET6
 * =============================================================================
 *
 *  This module is the single owner of IMU sensor reads and Kalman filters.
 *
 *  Yaw is calculated using GYRO ONLY (no magnetometer) to avoid calibration
 *  issues. Call IMU_Fusion_ResetYaw() after each maneuver to prevent drift.
 *
 *  Call sequence:
 *    1. IMU_Fusion_Init(&hi2c2)              — once at startup
 *    2. IMU_Fusion_SetGyroBias(500)          — once at startup (robot stationary)
 *    3. IMU_Fusion_Update()                  — every 10 ms
 *    4. IMU_Fusion_GetYaw() / GetPitch() etc — anytime, returns latest
 *    5. IMU_Fusion_ResetYaw()                — after each turn to prevent drift
 *
 * =============================================================================
 */

#include "imu_fusion.h"
#include "kalman.h"
#include <math.h>

#define RAD_TO_DEG  57.29577951f
#define IMU_DT      0.01f               /* 10 ms sample period */

/* ===========================================================================
 *  PRIVATE — State
 * =========================================================================== */

static I2C_HandleTypeDef *fusion_hi2c = NULL;
static ICM20948_Data_t    imu_data;

/* Kalman filter instances (pitch and roll only) */
static Kalman_t kalman_pitch;
static Kalman_t kalman_roll;

/* Filtered outputs */
static float filtered_pitch = 0.0f;
static float filtered_roll  = 0.0f;
static float filtered_yaw   = 0.0f;   /* Gyro-integrated yaw */

/* Gyro Z bias (calibrated at startup) */
static float gyro_z_bias = 0.0f;

/* Latest gyro Z rate (read every cycle) */
static float latest_gyro_z = 0.0f;


/* ===========================================================================
 *  PRIVATE — Reference angle helpers
 *
 *  AXIS MAPPING (based on physical testing):
 *    The ICM-20948 is mounted rotated 90° on this board:
 *      - IMU X axis → points RIGHT (board's Y direction)
 *      - IMU Y axis → points FORWARD (board's X direction, toward OLED)
 *      - IMU Z axis → points UP
 *
 *    For the robot (OLED = forward):
 *      - Pitch = tilt forward/backward → rotation around board's Y axis → IMU X axis
 *      - Roll  = tilt left/right      → rotation around board's X axis → IMU Y axis
 *      - Yaw   = turn left/right      → rotation around Z axis (unchanged)
 *
 *    Sign convention (matching standard aircraft/robot convention):
 *      - Pitch positive = nose up (tilt backward)
 *      - Roll positive  = right wing down (tilt right)
 *      - Yaw positive   = clockwise when viewed from above
 * =========================================================================== */

static float Accel_GetPitch(const ICM20948_Data_t *d)
{
    /* Pitch from IMU Y (board's forward direction) */
    return atan2f(d->accel_g.y, d->accel_g.z) * RAD_TO_DEG;
}

static float Accel_GetRoll(const ICM20948_Data_t *d)
{
    /* Roll from IMU X (board's right direction), negated for correct sign */
    return -atan2f(d->accel_g.x, d->accel_g.z) * RAD_TO_DEG;
}


/* ===========================================================================
 *  PUBLIC — Initialisation
 * =========================================================================== */

bool IMU_Fusion_Init(I2C_HandleTypeDef *hi2c)
{
    fusion_hi2c = hi2c;

    /* Initialise ICM-20948 (accel + gyro) */
    if (ICM20948_Init(hi2c, GYRO_FS_500DPS, ACCEL_FS_4G) != HAL_OK)
        return false;

    /* Initialise Kalman filters for pitch and roll */
    Kalman_Init(&kalman_pitch);
    Kalman_Init(&kalman_roll);

    /* Allow sensor to stabilise */
    HAL_Delay(100);

    /* Seed the filters with initial measurements */
    if (ICM20948_ReadAll(hi2c, &imu_data) == HAL_OK)
    {
        float init_pitch = Accel_GetPitch(&imu_data);
        float init_roll  = Accel_GetRoll(&imu_data);

        Kalman_SetAngle(&kalman_pitch, init_pitch);
        Kalman_SetAngle(&kalman_roll,  init_roll);

        filtered_pitch = init_pitch;
        filtered_roll  = init_roll;
    }

    /* Initialize yaw to zero */
    filtered_yaw = 0.0f;
    gyro_z_bias = 0.0f;

    return true;
}


/* ===========================================================================
 *  PUBLIC — Gyro Bias Calibration
 *
 *  Call this once at startup with the robot STATIONARY.
 *  Averages gyro readings to find the zero-point offset.
 * =========================================================================== */

void IMU_Fusion_SetGyroBias(uint16_t samples)
{
    if (fusion_hi2c == NULL) return;

    float total = 0.0f;

    for (uint16_t i = 0; i < samples; i++)
    {
        if (ICM20948_ReadAll(fusion_hi2c, &imu_data) == HAL_OK)
        {
            /* Negate here to match the negation in IMU_Fusion_Update() */
            total += (-imu_data.gyro_dps.z);
        }
        HAL_Delay(2);
    }

    gyro_z_bias = total / (float)samples;
}


/* ===========================================================================
 *  PUBLIC — Fusion update (call every ~10 ms)
 * =========================================================================== */

static uint32_t last_update_tick = 0;

void IMU_Fusion_Update(void)
{
    if (fusion_hi2c == NULL) return;

    /* Calculate actual elapsed time */
    uint32_t now = HAL_GetTick();
    float dt;
    
    if (last_update_tick == 0)
    {
        /* First call — use default */
        dt = IMU_DT;
    }
    else
    {
        /* Calculate actual time since last update */
        dt = (now - last_update_tick) / 1000.0f;  /* Convert ms to seconds */
        
        /* Sanity check: clamp to reasonable range */
        if (dt < 0.001f) dt = 0.001f;   /* Min 1ms */
        if (dt > 0.1f)   dt = 0.1f;     /* Max 100ms */
    }
    last_update_tick = now;

    if (ICM20948_ReadAll(fusion_hi2c, &imu_data) != HAL_OK)
        return;

    /* -------------------------------------------------------------------------
     *  Pitch and Roll: Kalman filter (accel + gyro)
     * ------------------------------------------------------------------------- */
    float accel_pitch = Accel_GetPitch(&imu_data);
    float accel_roll  = Accel_GetRoll(&imu_data);

    /*
     * Gyro rates (with axis remapping and sign correction):
     *   - Pitch rate = rotation around board's Y axis → IMU gyro X (negated)
     *   - Roll rate  = rotation around board's X axis → IMU gyro Y (negated)
     */
    float gyro_pitch_rate = -imu_data.gyro_dps.x;
    float gyro_roll_rate  = -imu_data.gyro_dps.y;

    /* Run Kalman filters for pitch and roll (use actual dt) */
    filtered_pitch = Kalman_Update(&kalman_pitch, accel_pitch, gyro_pitch_rate, dt);
    filtered_roll  = Kalman_Update(&kalman_roll,  accel_roll,  gyro_roll_rate,  dt);

    /* -------------------------------------------------------------------------
     *  Yaw: Gyro integration only (no magnetometer)
     * ------------------------------------------------------------------------- */
    
    /* Negate raw gyro Z for sign convention (positive = clockwise) */
    float raw_gyro_z = -imu_data.gyro_dps.z;
    
    /* Subtract bias to get corrected rate */
    float gyro_yaw_rate = raw_gyro_z - gyro_z_bias;
    
    /* Integrate gyro rate to get angle change (use actual dt) */
    filtered_yaw += gyro_yaw_rate * dt;

    /* Normalize yaw to -180 to +180 */
    if (filtered_yaw > 180.0f)  filtered_yaw -= 360.0f;
    if (filtered_yaw < -180.0f) filtered_yaw += 360.0f;

    /* Store latest gyro Z for consumers that need rate info */
    latest_gyro_z = gyro_yaw_rate;
}


/* ===========================================================================
 *  PUBLIC — Getters
 * =========================================================================== */

float IMU_Fusion_GetYaw(void)   { return filtered_yaw;   }
float IMU_Fusion_GetPitch(void) { return filtered_pitch; }
float IMU_Fusion_GetRoll(void)  { return filtered_roll;  }
float IMU_Fusion_GetGyroZ(void) { return latest_gyro_z;  }

const ICM20948_Data_t* IMU_Fusion_GetRawData(void)
{
    return &imu_data;
}


/* ===========================================================================
 *  PUBLIC — Yaw Control
 *
 *  Since we're using gyro-only yaw, it will drift over time.
 *  Use these functions to reset or set yaw as needed.
 * =========================================================================== */

void IMU_Fusion_ResetYaw(void)
{
    filtered_yaw = 0.0f;
}

void IMU_Fusion_SetYaw(float yaw)
{
    filtered_yaw = yaw;
}

float IMU_Fusion_GetGyroBias(void)
{
    return gyro_z_bias;
}