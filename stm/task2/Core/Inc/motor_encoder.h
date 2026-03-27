/**
 * =============================================================================
 *  motor_encoder.h — Motor Control with Encoder Feedback & RPM Calculation
 *  Board: WHEELTEC STM32F407VET6 (Rev 23.0)
 * =============================================================================
 */

#ifndef MOTOR_ENCODER_H
#define MOTOR_ENCODER_H

#include "main.h"   /* Brings in HAL, GPIO definitions, timer handles */
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ===========================================================================
 *  USER-CONFIGURABLE ENCODER PARAMETERS
 *  *** Adjust these to match YOUR motor/encoder specs ***
 * =========================================================================== */

/**
 * Pulses Per Revolution of the encoder (before quadrature multiplication).
 * Common values: 7, 11, 13 for small hobby motors.
 * Check your motor's datasheet.
 */
#define ENCODER_PPR         11

/**
 * Gear ratio of the motor gearbox.
 * e.g., 30 means 30 motor shaft turns = 1 output shaft turn.
 * Set to 1 if there is no gearbox.
 */
#define GEAR_RATIO          30

/**
 * Total encoder counts per output shaft revolution.
 * In quadrature encoder mode (both edges, both channels), the STM32
 * counts 4 edges per encoder pulse. So:
 *   counts_per_rev = PPR × 4 × GEAR_RATIO
 *
 * Example: 11 PPR × 4 × 30 = 1320 counts per output shaft revolution
 */
#define COUNTS_PER_REV      (ENCODER_PPR * 4 * GEAR_RATIO)

/**
 * Sampling period in milliseconds.
 * This must match your TIM6 configuration.
 */
#define SAMPLE_PERIOD_MS    10

/**
 * Maximum PWM compare value (= TIM ARR value).
 * With Prescaler=83, ARR=999 on TIM4 (84 MHz APB1): PWM freq ≈ 1 kHz
 * With Prescaler=167, ARR=999 on TIM9 (168 MHz APB2): PWM freq ≈ 1 kHz
 */
#define MOTOR_PWM_MAX       999


/* ===========================================================================
 *  MOTOR IDENTIFIERS
 * =========================================================================== */

typedef enum {
    MOTOR_A = 0,    /* TIM4 PWM,  TIM2 Encoder  */
    MOTOR_B = 1,    /* TIM9 PWM,  TIM3 Encoder  */
} Motor_ID_t;


/* ===========================================================================
 *  CubeMX CONFIGURATION CHEAT-SHEET
 * ===========================================================================
 *
 *  Configure these in STM32CubeIDE's .ioc file (CubeMX graphical editor):
 *
 *  ┌─────────────────────────────────────────────────────────────────────┐
 *  │  MOTOR A PWM — TIM4                                               │
 *  │    Clock Source:    Internal Clock                                 │
 *  │    Channel 3:      PWM Generation CH3   (pin PB8)                 │
 *  │    Channel 4:      PWM Generation CH4   (pin PB9)                 │
 *  │    Prescaler:      83        (84 MHz APB1 / 84 = 1 MHz tick)     │
 *  │    Counter Period:  999      (1 MHz / 1000 = 1 kHz PWM)          │
 *  │    Auto-Reload:    Enable                                         │
 *  ├─────────────────────────────────────────────────────────────────────┤
 *  │  MOTOR B PWM — TIM9                                               │
 *  │    Clock Source:    Internal Clock                                 │
 *  │    Channel 1:      PWM Generation CH1   (pin PE5)                 │
 *  │    Channel 2:      PWM Generation CH2   (pin PE6)                 │
 *  │    Prescaler:      167       (168 MHz APB2 / 168 = 1 MHz tick)   │
 *  │    Counter Period:  999      (1 MHz / 1000 = 1 kHz PWM)          │
 *  │    Auto-Reload:    Enable                                         │
 *  ├─────────────────────────────────────────────────────────────────────┤
 *  │  MOTOR A ENCODER — TIM2                                           │
 *  │    Combined Channels: Encoder Mode                                │
 *  │    Encoder Mode:      Encoder Mode TI1 and TI2  (both edges)     │
 *  │    Prescaler:         0                                           │
 *  │    Counter Period:    0xFFFFFFFF  (TIM2 is 32-bit)               │
 *  │    Input Filter:      5  (noise filtering, adjust if needed)      │
 *  │    Polarity:          Rising (both channels)                      │
 *  │    Mapped Pins:       CH1 = PA15,  CH2 = PB3                     │
 *  ├─────────────────────────────────────────────────────────────────────┤
 *  │  MOTOR B ENCODER — TIM3                                           │
 *  │    Combined Channels: Encoder Mode                                │
 *  │    Encoder Mode:      Encoder Mode TI1 and TI2  (both edges)     │
 *  │    Prescaler:         0                                           │
 *  │    Counter Period:    0xFFFF  (TIM3 is 16-bit)                   │
 *  │    Input Filter:      5                                           │
 *  │    Polarity:          Rising (both channels)                      │
 *  │    Mapped Pins:       CH1 = PB4,   CH2 = PB5                     │
 *  ├─────────────────────────────────────────────────────────────────────┤
 *  │  SAMPLING TIMER — TIM6                                            │
 *  │    Clock Source:    Internal Clock                                 │
 *  │    Prescaler:       8399     (84 MHz / 8400 = 10 kHz tick)       │
 *  │    Counter Period:  99       (10 kHz / 100 = 100 Hz = 10 ms)     │
 *  │    Auto-Reload:     Enable                                        │
 *  │    NVIC:            TIM6 global interrupt → ENABLED               │
 *  └─────────────────────────────────────────────────────────────────────┘
 *
 *  PIN ASSIGNMENT VERIFICATION (Pinout & Configuration tab):
 *    PA15 → TIM2_CH1    PB3  → TIM2_CH2     (Motor A encoder)
 *    PB4  → TIM3_CH1    PB5  → TIM3_CH2     (Motor B encoder)
 *    PB8  → TIM4_CH3    PB9  → TIM4_CH4     (Motor A PWM)
 *    PE5  → TIM9_CH1    PE6  → TIM9_CH2     (Motor B PWM)
 *
 * =========================================================================== */


/* ===========================================================================
 *  PUBLIC API
 * =========================================================================== */

/* ---- Initialisation (call once in main before the while loop) ---- */
void Encoder_Init(void);
void Motor_PWM_Init(void);
void Encoder_Sampling_Timer_Init(void);

/* ---- Motor control ---- */
void MotorA_Forward(uint16_t speed);
void MotorA_Reverse(uint16_t speed);
void MotorA_Stop(void);
void MotorA_Brake(void);

void MotorB_Forward(uint16_t speed);
void MotorB_Reverse(uint16_t speed);
void MotorB_Stop(void);
void MotorB_Brake(void);

void Motor_SetSpeed(Motor_ID_t motor, int16_t speed);

/* ---- Encoder reading ---- */
int32_t Encoder_GetCount(Motor_ID_t motor);
int32_t Encoder_GetDelta(Motor_ID_t motor);
float   Encoder_GetRPM(Motor_ID_t motor);
void    Encoder_Reset(Motor_ID_t motor);

#ifdef __cplusplus
}
#endif

#endif /* MOTOR_ENCODER_H */