/**
 * =============================================================================
 *  ultrasonic.h — HC-SR04 Ultrasonic Sensor Driver (Non-Blocking)
 *  Board: WHEELTEC STM32F407VET6
 * =============================================================================
 *
 *  HARDWARE:
 *    TRIG  — PB15 (GPIO output)
 *    ECHO  — PB14 (TIM12 CH1 input capture, both edges)
 *
 *  USAGE:
 *    1. Call Ultrasonic_Init() after TIM7 and TIM12 are initialised.
 *    2. Call Ultrasonic_Process() every main loop iteration — it manages
 *       the trigger/wait cycle internally without blocking.
 *    3. Read the latest distance with Ultrasonic_GetDistance_cm().
 *
 *    The echo pulse is measured asynchronously via the TIM12 input capture
 *    interrupt. Call Ultrasonic_CaptureCallback() from HAL_TIM_IC_CaptureCallback.
 *
 * =============================================================================
 */

#ifndef ULTRASONIC_H
#define ULTRASONIC_H

#include "stm32f4xx_hal.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief  Initialise the ultrasonic module.
 * @param  htim_us   Timer used for microsecond delay (TIM7, 1 µs tick).
 * @param  htim_echo Timer used for echo input capture (TIM12).
 */
void Ultrasonic_Init(TIM_HandleTypeDef *htim_us, TIM_HandleTypeDef *htim_echo);

/**
 * @brief  Non-blocking process function — call every main loop iteration.
 *         Manages the trigger pulse and measurement wait internally.
 */
void Ultrasonic_Process(void);

/**
 * @brief  Get the latest measured distance.
 * @return Distance in centimetres, or 0.0 if no valid echo received.
 */
float Ultrasonic_GetDistance_cm(void);

/**
 * @brief  Get the raw echo pulse width.
 * @return Pulse width in microseconds.
 */
float Ultrasonic_GetEcho_us(void);

/**
 * @brief  TIM input capture callback — call from HAL_TIM_IC_CaptureCallback.
 * @param  htim  Timer handle passed by HAL.
 */
void Ultrasonic_CaptureCallback(TIM_HandleTypeDef *htim);

#ifdef __cplusplus
}
#endif

#endif /* ULTRASONIC_H */