/**
 * =============================================================================
 *  ir_sensor.h — GP2Y0A21YK0F IR Distance Sensor Driver
 *  Board: WHEELTEC STM32F407VET6
 * =============================================================================
 *
 *  HARDWARE:
 *    IR Left   — PA2 (ADC1 IN2)
 *    IR Right  — PA3 (ADC1 IN3)
 *
 *  USAGE:
 *    1. Configure ADC1 with IN2 and IN3 in CubeMX:
 *       - Scan Conversion Mode: Enabled
 *       - Continuous Conversion Mode: Enabled
 *       - DMA Continuous Requests: Enabled
 *       - DMA: Circular mode, Half Word
 *       - Number of Conversions: 2
 *       - Rank 1: Channel 2, 144 cycles
 *       - Rank 2: Channel 3, 144 cycles
 *
 *    2. Call IR_Sensor_Init() after MX_ADC1_Init() and MX_DMA_Init().
 *    3. Read distances with IR_Sensor_GetDistance_cm().
 *       Values update continuously via DMA — no polling needed.
 *
 *  SENSOR:
 *    GP2Y0A21YK0F (Sharp) — effective range 10–80 cm.
 *    Output voltage is inversely proportional to distance.
 *    Formula: distance_cm = 27.0 / (voltage - 0.1) - 2.0
 *
 * =============================================================================
 */

#ifndef IR_SENSOR_H
#define IR_SENSOR_H

#include "stm32f4xx_hal.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ===========================================================================
 *  SENSOR IDENTIFIERS
 * =========================================================================== */

typedef enum {
    IR_LEFT = 0,    /* ADC1 IN2 (PA2) — Rank 1 → ADC_VAL[0] */
    IR_RIGHT,       /* ADC1 IN3 (PA3) — Rank 2 → ADC_VAL[1] */
    IR_COUNT
} IR_Sensor_ID_t;

/* ===========================================================================
 *  PUBLIC API
 * =========================================================================== */

/**
 * @brief  Initialise the IR sensor module and start DMA conversions.
 * @param  hadc  Pointer to the ADC handle (hadc1).
 */
void IR_Sensor_Init(ADC_HandleTypeDef *hadc);

/**
 * @brief  Get the latest distance reading from an IR sensor.
 * @param  sensor  IR_LEFT or IR_RIGHT
 * @return Distance in centimetres (valid range ~10–80 cm).
 *         Returns 0.0 if the reading is out of range.
 */
float IR_Sensor_GetDistance_cm(IR_Sensor_ID_t sensor);

/**
 * @brief  Get the raw ADC value from an IR sensor.
 * @param  sensor  IR_LEFT or IR_RIGHT
 * @return Raw 12-bit ADC value (0–4095).
 */
uint16_t IR_Sensor_GetRaw(IR_Sensor_ID_t sensor);

#ifdef __cplusplus
}
#endif

#endif /* IR_SENSOR_H */