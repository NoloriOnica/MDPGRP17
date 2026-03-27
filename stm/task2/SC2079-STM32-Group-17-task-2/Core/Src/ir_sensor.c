/**
 * =============================================================================
 *  ir_sensor.c — GP2Y0A21YK0F IR Distance Sensor Driver
 *  Board: WHEELTEC STM32F407VET6
 * =============================================================================
 *
 *  HARDWARE:
 *    IR Left   — PA2 (ADC1 IN2)
 *    IR Right  — PA3 (ADC1 IN3)
 *
 *  HOW IT WORKS:
 *    ADC1 runs in continuous scan mode with DMA (circular). The DMA
 *    transfers each conversion result into adc_buf[] automatically.
 *    No CPU intervention is needed after init — the buffer is always
 *    up to date when read.
 *
 *  DISTANCE FORMULA (GP2Y0A21YK0F):
 *    voltage    = raw * 3.3 / 4095
 *    distance   = 27.0 / (voltage - 0.1) - 2.0   [cm]
 *
 *    Valid output range: ~10–80 cm. Outside this range the sensor
 *    output is unreliable, so readings are clamped.
 *
 * =============================================================================
 */

#include "ir_sensor.h"

/* ===========================================================================
 *  CONFIGURATION
 * =========================================================================== */

/* ADC reference voltage and resolution */
#define ADC_VREF        3.3f
#define ADC_MAX_VAL     4095.0f

/* GP2Y0A21YK0F curve-fit constants:
 *   distance_cm = COEFF_A / (voltage - COEFF_B) - COEFF_C
 * Adjust these if your sensor reads consistently off. */
#define COEFF_A         27.0f
#define COEFF_B         0.1f
#define COEFF_C         2.0f

/* Valid distance range — readings outside this are unreliable */
#define DIST_MIN_CM     4.0f
#define DIST_MAX_CM     80.0f

/* ===========================================================================
 *  PRIVATE STATE
 * =========================================================================== */

static uint16_t adc_buf[IR_COUNT];   /* DMA target buffer */


/* ===========================================================================
 *  PRIVATE — Convert raw ADC to distance
 * =========================================================================== */

static float RawToDistance(uint16_t raw)
{
    float voltage = (float)raw * ADC_VREF / ADC_MAX_VAL;

    /* Avoid division by zero or near-zero */
    if (voltage <= COEFF_B + 0.01f)
        return 0.0f;

    float dist = COEFF_A / (voltage - COEFF_B) - COEFF_C;

    /* Clamp to valid range */
    if (dist < DIST_MIN_CM)
        return 0.0f;
    else if (dist > DIST_MAX_CM)
        return -1.0f;

    return dist;
}


/* ===========================================================================
 *  PUBLIC — Initialisation
 * =========================================================================== */

void IR_Sensor_Init(ADC_HandleTypeDef *hadc)
{
    HAL_ADC_Start_DMA(hadc, (uint32_t *)adc_buf, IR_COUNT);
}


/* ===========================================================================
 *  PUBLIC — Distance getter
 * =========================================================================== */

float IR_Sensor_GetDistance_cm(IR_Sensor_ID_t sensor)
{
    if (sensor >= IR_COUNT)
        return 0.0f;

    return RawToDistance(adc_buf[sensor]);
}


/* ===========================================================================
 *  PUBLIC — Raw ADC getter
 * =========================================================================== */

uint16_t IR_Sensor_GetRaw(IR_Sensor_ID_t sensor)
{
    if (sensor >= IR_COUNT)
        return 0;

    return adc_buf[sensor];
}