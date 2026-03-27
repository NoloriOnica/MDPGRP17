/**
 * =============================================================================
 *  icm20948.c — ICM-20948 IMU Driver (Accelerometer + Gyroscope only)
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
 * =============================================================================
 */

#include "icm20948.h"

/* ===========================================================================
 *  PRIVATE — Current configuration
 * =========================================================================== */
static float accel_sensitivity = ACCEL_SENSITIVITY_2G;
static float gyro_sensitivity  = GYRO_SENSITIVITY_250DPS;

/* ===========================================================================
 *  PRIVATE — Low-level I2C helpers
 * =========================================================================== */

static HAL_StatusTypeDef ICM20948_WriteReg(I2C_HandleTypeDef *hi2c,
                                           uint8_t reg, uint8_t value)
{
    return HAL_I2C_Mem_Write(hi2c, ICM20948_I2C_ADDR,
                             reg, I2C_MEMADD_SIZE_8BIT,
                             &value, 1, 100);
}

static HAL_StatusTypeDef ICM20948_ReadReg(I2C_HandleTypeDef *hi2c,
                                          uint8_t reg, uint8_t *value)
{
    return HAL_I2C_Mem_Read(hi2c, ICM20948_I2C_ADDR,
                            reg, I2C_MEMADD_SIZE_8BIT,
                            value, 1, 100);
}

static HAL_StatusTypeDef ICM20948_ReadRegs(I2C_HandleTypeDef *hi2c,
                                           uint8_t reg, uint8_t *buf,
                                           uint16_t len)
{
    return HAL_I2C_Mem_Read(hi2c, ICM20948_I2C_ADDR,
                            reg, I2C_MEMADD_SIZE_8BIT,
                            buf, len, 100);
}

static HAL_StatusTypeDef ICM20948_SetBank(I2C_HandleTypeDef *hi2c,
                                          uint8_t bank)
{
    uint8_t bank_val = (bank & 0x03) << 4;
    return ICM20948_WriteReg(hi2c, REG_BANK_SEL, bank_val);
}


/* ===========================================================================
 *  PUBLIC — Initialisation
 * =========================================================================== */

HAL_StatusTypeDef ICM20948_Init(I2C_HandleTypeDef *hi2c,
                                ICM20948_GyroFS_t gyro_fs,
                                ICM20948_AccelFS_t accel_fs)
{
    HAL_StatusTypeDef status;

    /* ------------------------------------------------------------------
     *  Step 1: Verify WHO_AM_I
     * ------------------------------------------------------------------ */
    ICM20948_SetBank(hi2c, 0);

    uint8_t who = ICM20948_WhoAmI(hi2c);
    if (who != ICM20948_WHO_AM_I_VAL)
        return HAL_ERROR;

    /* ------------------------------------------------------------------
     *  Step 2: Reset the device
     * ------------------------------------------------------------------ */
    ICM20948_SetBank(hi2c, 0);
    status = ICM20948_WriteReg(hi2c, B0_PWR_MGMT_1, PWR_MGMT_1_RESET);
    if (status != HAL_OK) return status;
    HAL_Delay(100);

    /* ------------------------------------------------------------------
     *  Step 3: Wake up, auto clock
     * ------------------------------------------------------------------ */
    ICM20948_SetBank(hi2c, 0);
    status = ICM20948_WriteReg(hi2c, B0_PWR_MGMT_1, PWR_MGMT_1_CLK_AUTO);
    if (status != HAL_OK) return status;
    HAL_Delay(50);

    /* ------------------------------------------------------------------
     *  Step 4: Enable accel and gyro (all axes)
     * ------------------------------------------------------------------ */
    ICM20948_SetBank(hi2c, 0);
    status = ICM20948_WriteReg(hi2c, B0_PWR_MGMT_2, 0x00);
    if (status != HAL_OK) return status;

    /* ------------------------------------------------------------------
     *  Step 5: Configure gyroscope (Bank 2)
     * ------------------------------------------------------------------ */
    ICM20948_SetBank(hi2c, 2);

    uint8_t gyro_config = (uint8_t)gyro_fs | 0x01;  /* Enable DLPF */
    status = ICM20948_WriteReg(hi2c, B2_GYRO_CONFIG_1, gyro_config);
    if (status != HAL_OK) return status;

    status = ICM20948_WriteReg(hi2c, B2_GYRO_SMPLRT_DIV, 0x00);  /* Max sample rate */
    if (status != HAL_OK) return status;

    switch (gyro_fs)
    {
        case GYRO_FS_250DPS:  gyro_sensitivity = GYRO_SENSITIVITY_250DPS;  break;
        case GYRO_FS_500DPS:  gyro_sensitivity = GYRO_SENSITIVITY_500DPS;  break;
        case GYRO_FS_1000DPS: gyro_sensitivity = GYRO_SENSITIVITY_1000DPS; break;
        case GYRO_FS_2000DPS: gyro_sensitivity = GYRO_SENSITIVITY_2000DPS; break;
    }

    /* ------------------------------------------------------------------
     *  Step 6: Configure accelerometer (Bank 2)
     * ------------------------------------------------------------------ */
    uint8_t accel_config = (uint8_t)accel_fs | 0x01;  /* Enable DLPF */
    status = ICM20948_WriteReg(hi2c, B2_ACCEL_CONFIG, accel_config);
    if (status != HAL_OK) return status;

    status = ICM20948_WriteReg(hi2c, B2_ACCEL_SMPLRT_DIV_1, 0x00);  /* Max sample rate */
    if (status != HAL_OK) return status;
    status = ICM20948_WriteReg(hi2c, B2_ACCEL_SMPLRT_DIV_2, 0x00);
    if (status != HAL_OK) return status;

    switch (accel_fs)
    {
        case ACCEL_FS_2G:  accel_sensitivity = ACCEL_SENSITIVITY_2G;  break;
        case ACCEL_FS_4G:  accel_sensitivity = ACCEL_SENSITIVITY_4G;  break;
        case ACCEL_FS_8G:  accel_sensitivity = ACCEL_SENSITIVITY_8G;  break;
        case ACCEL_FS_16G: accel_sensitivity = ACCEL_SENSITIVITY_16G; break;
    }

    /* ------------------------------------------------------------------
     *  Step 7: Return to Bank 0
     * ------------------------------------------------------------------ */
    ICM20948_SetBank(hi2c, 0);

    return HAL_OK;
}


/* ===========================================================================
 *  PUBLIC — WHO_AM_I check
 * =========================================================================== */

uint8_t ICM20948_WhoAmI(I2C_HandleTypeDef *hi2c)
{
    uint8_t who = 0;
    ICM20948_SetBank(hi2c, 0);
    ICM20948_ReadReg(hi2c, B0_WHO_AM_I, &who);
    return who;
}


/* ===========================================================================
 *  PUBLIC — Read all sensor data (accel + gyro + temp)
 * =========================================================================== */

HAL_StatusTypeDef ICM20948_ReadAll(I2C_HandleTypeDef *hi2c,
                                   ICM20948_Data_t *data)
{
    HAL_StatusTypeDef status;
    uint8_t buf[14];

    /*
     * Read 14 bytes: accel (6) + gyro (6) + temp (2)
     */
    ICM20948_SetBank(hi2c, 0);
    status = ICM20948_ReadRegs(hi2c, B0_ACCEL_XOUT_H, buf, 14);
    if (status != HAL_OK) return status;

    /* --- Parse accel (big-endian) --- */
    data->accel_raw.x = (int16_t)((buf[0]  << 8) | buf[1]);
    data->accel_raw.y = (int16_t)((buf[2]  << 8) | buf[3]);
    data->accel_raw.z = (int16_t)((buf[4]  << 8) | buf[5]);

    /* --- Parse gyro (big-endian) --- */
    data->gyro_raw.x  = (int16_t)((buf[6]  << 8) | buf[7]);
    data->gyro_raw.y  = (int16_t)((buf[8]  << 8) | buf[9]);
    data->gyro_raw.z  = (int16_t)((buf[10] << 8) | buf[11]);

    /* --- Parse temp (big-endian) --- */
    data->temp_raw = (int16_t)((buf[12] << 8) | buf[13]);

    /* --- Convert to physical units --- */
    data->accel_g.x = (float)data->accel_raw.x / accel_sensitivity;
    data->accel_g.y = (float)data->accel_raw.y / accel_sensitivity;
    data->accel_g.z = (float)data->accel_raw.z / accel_sensitivity;

    data->gyro_dps.x = (float)data->gyro_raw.x / gyro_sensitivity;
    data->gyro_dps.y = (float)data->gyro_raw.y / gyro_sensitivity;
    data->gyro_dps.z = (float)data->gyro_raw.z / gyro_sensitivity;

    data->temp_c = ((float)data->temp_raw / TEMP_SENSITIVITY) + TEMP_OFFSET;

    return HAL_OK;
}


/* ===========================================================================
 *  PUBLIC — Read accelerometer only
 * =========================================================================== */

HAL_StatusTypeDef ICM20948_ReadAccel(I2C_HandleTypeDef *hi2c,
                                     ICM20948_Data_t *data)
{
    HAL_StatusTypeDef status;
    uint8_t buf[6];

    ICM20948_SetBank(hi2c, 0);
    status = ICM20948_ReadRegs(hi2c, B0_ACCEL_XOUT_H, buf, 6);
    if (status != HAL_OK) return status;

    data->accel_raw.x = (int16_t)((buf[0] << 8) | buf[1]);
    data->accel_raw.y = (int16_t)((buf[2] << 8) | buf[3]);
    data->accel_raw.z = (int16_t)((buf[4] << 8) | buf[5]);

    data->accel_g.x = (float)data->accel_raw.x / accel_sensitivity;
    data->accel_g.y = (float)data->accel_raw.y / accel_sensitivity;
    data->accel_g.z = (float)data->accel_raw.z / accel_sensitivity;

    return HAL_OK;
}


/* ===========================================================================
 *  PUBLIC — Read gyroscope only
 * =========================================================================== */

HAL_StatusTypeDef ICM20948_ReadGyro(I2C_HandleTypeDef *hi2c,
                                    ICM20948_Data_t *data)
{
    HAL_StatusTypeDef status;
    uint8_t buf[6];

    ICM20948_SetBank(hi2c, 0);
    status = ICM20948_ReadRegs(hi2c, B0_GYRO_XOUT_H, buf, 6);
    if (status != HAL_OK) return status;

    data->gyro_raw.x = (int16_t)((buf[0] << 8) | buf[1]);
    data->gyro_raw.y = (int16_t)((buf[2] << 8) | buf[3]);
    data->gyro_raw.z = (int16_t)((buf[4] << 8) | buf[5]);

    data->gyro_dps.x = (float)data->gyro_raw.x / gyro_sensitivity;
    data->gyro_dps.y = (float)data->gyro_raw.y / gyro_sensitivity;
    data->gyro_dps.z = (float)data->gyro_raw.z / gyro_sensitivity;

    return HAL_OK;
}