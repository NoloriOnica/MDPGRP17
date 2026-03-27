/**
 * =============================================================================
 *  servo.h — Servo Motor Driver for TD-8120MG
 *  Board: WHEELTEC STM32F407VET6 (Rev 23.0)
 * =============================================================================
 *
 *  Wiring (from schematic Sheet 2, Header J1):
 *    Pin 1 = PC6  → TIM8_CH1 (PWM signal)
 *    Pin 2 = 5V5  (power)
 *    Pin 3 = GND
 *
 *  Servo Specs (TD-8120MG datasheet):
 *    PWM frequency:    50 Hz (20 ms period)
 *    Pulse width:      500 µs (0°) to 2500 µs (180°)
 *    Steering range:   Ticks 65–85 (practical working range)
 *    Max current:      2500 mA at stall
 *
 *  CubeMX Configuration:
 *    TIM8:
 *      Clock Source:     Internal Clock
 *      Channel 1:       PWM Generation CH1  (pin PC6)
 *      Prescaler:       3359    (168 MHz / 3360 = 50 kHz → 20 µs/tick)
 *      Counter Period:  999     (1000 × 20 µs = 20 ms = 50 Hz)
 *      Auto-Reload:     Enable
 *
 *  Tick-to-angle mapping (20 µs per tick):
 *      25 ticks  =  500 µs  =   0 degrees
 *      75 ticks  = 1500 µs  =  90 degrees (centre)
 *     125 ticks  = 2500 µs  = 180 degrees
 *
 * =============================================================================
 */

#ifndef SERVO_H
#define SERVO_H

#include "main.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ===========================================================================
 *  SERVO TICK LIMITS
 * =========================================================================== */
#define SERVO_TICK_MIN      25      /* 500 µs  =   0 degrees */
#define SERVO_TICK_CENTER   61      /* 1500 µs =  90 degrees */
#define SERVO_TICK_MAX      125     /* 2500 µs = 180 degrees */

/* Practical steering range (from datasheet) */
#define SERVO_STEER_MIN     33 // 50
#define SERVO_STEER_MAX     120

/* ===========================================================================
 *  PUBLIC API
 * =========================================================================== */

/**
 * @brief  Initialise servo PWM output on TIM8 CH1.
 *         Sets servo to centre position.
 *         Call once after MX_TIM8_Init() in main().
 */
void Servo_Init(void);

/**
 * @brief  Set servo position by raw tick value.
 * @param  tick  PWM compare value [25 .. 125]
 *               25 = 0°, 75 = 90°, 125 = 180°
 */
void Servo_SetTick(uint16_t tick);

/**
 * @brief  Set servo position by angle.
 * @param  angle  Desired angle in degrees [0.0 .. 180.0]
 */
void Servo_SetAngle(float angle);

/**
 * @brief  Set steering position with normalised input.
 * @param  steer  Normalised steering [-1.0 .. +1.0]
 *                -1.0 = full left, 0.0 = centre, +1.0 = full right
 *                Maps to the practical steering range (ticks 65–85)
 */
void Servo_SetSteering(float steer);

/**
 * @brief  Get the current servo tick value.
 * @return Current PWM compare value
 */
uint16_t Servo_GetTick(void);

#ifdef __cplusplus
}
#endif

#endif /* SERVO_H */