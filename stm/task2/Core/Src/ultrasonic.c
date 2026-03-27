/**
 * =============================================================================
 *  ultrasonic.c — HC-SR04 Ultrasonic Sensor Driver (Non-Blocking)
 *  Board: WHEELTEC STM32F407VET6
 * =============================================================================
 *
 *  HARDWARE:
 *    TRIG  — PB15 (GPIO output)
 *    ECHO  — PB14 (TIM12 CH1 input capture, both edges)
 *
 *  TIMING:
 *    TIM12 prescaler = 84-1 → 1 µs per tick (APB1 = 84 MHz).
 *    TIM12 period    = 65535 → max measurable pulse ~65 ms (~11 m).
 *    TIM7  is used for the 10 µs trigger pulse delay.
 *
 *  NON-BLOCKING DESIGN:
 *    Ultrasonic_Process() runs a state machine that cycles through:
 *      IDLE → TRIGGER → WAIT_ECHO → IDLE
 *    Each state transition is time-based using HAL_GetTick(), so the
 *    main loop is never blocked and IMU updates continue uninterrupted.
 *
 *    Measurement cycle: ~60 ms (configurable via US_CYCLE_MS).
 *
 * =============================================================================
 */

#include "ultrasonic.h"

/* ===========================================================================
 *  HARDWARE DEFINES
 * =========================================================================== */

#define TRIG_PORT       GPIOB
#define TRIG_PIN        GPIO_PIN_15

#define ECHO_PORT       GPIOB
#define ECHO_PIN        GPIO_PIN_14

/* Speed of sound: 0.0343 cm/µs (at ~20 °C) */
#define SPEED_OF_SOUND_CM_PER_US  0.0343f

/* Time between measurements in ms.
 * Must be long enough for the echo to return (~30 ms for 5 m max range).
 * 60 ms gives ~16 Hz update rate with margin. */
#define US_CYCLE_MS     60

/* ===========================================================================
 *  STATE MACHINE
 * =========================================================================== */

typedef enum {
    US_IDLE,        /* Waiting for next measurement cycle      */
    US_TRIGGER,     /* Trigger pin held high for 10 µs         */
    US_WAIT_ECHO,   /* Waiting for echo capture to complete    */
} US_State_t;

/* ===========================================================================
 *  PRIVATE STATE
 * =========================================================================== */

static TIM_HandleTypeDef *htim_delay = NULL;   /* TIM7 — microsecond delay  */
static TIM_HandleTypeDef *htim_capture = NULL; /* TIM12 — echo input capture */

static volatile uint32_t tc1 = 0;             /* Capture value at rising edge  */
static volatile uint32_t tc2 = 0;             /* Capture value at falling edge */
static volatile float    echo_us = 0.0f;      /* Measured pulse width in µs    */

static US_State_t us_state = US_IDLE;
static uint32_t   us_tick  = 0;               /* Timestamp for state transitions */


/* ===========================================================================
 *  PUBLIC — Initialisation
 * =========================================================================== */

void Ultrasonic_Init(TIM_HandleTypeDef *htim_us, TIM_HandleTypeDef *htim_echo)
{
    htim_delay   = htim_us;
    htim_capture = htim_echo;

    /* Start the microsecond delay timer (free-running) */
    HAL_TIM_Base_Start(htim_delay);

    /* Start echo input capture with interrupt */
    HAL_TIM_IC_Start_IT(htim_capture, TIM_CHANNEL_1);

    /* Ensure trigger pin starts low */
    HAL_GPIO_WritePin(TRIG_PORT, TRIG_PIN, GPIO_PIN_RESET);

    us_state = US_IDLE;
    us_tick  = HAL_GetTick();
}


/* ===========================================================================
 *  PUBLIC — Non-blocking process (call every main loop iteration)
 * =========================================================================== */

void Ultrasonic_Process(void)
{
    switch (us_state)
    {
        case US_IDLE:
            /* Start a new measurement cycle every US_CYCLE_MS */
            if (HAL_GetTick() - us_tick >= US_CYCLE_MS)
            {
                /* Send 10 µs trigger pulse.
                 * The 10 µs blocking delay is acceptable — it's only 10 µs,
                 * not 100 ms like before. */
                HAL_GPIO_WritePin(TRIG_PORT, TRIG_PIN, GPIO_PIN_SET);
                __HAL_TIM_SET_COUNTER(htim_delay, 0);
                while (__HAL_TIM_GET_COUNTER(htim_delay) < 10);
                HAL_GPIO_WritePin(TRIG_PORT, TRIG_PIN, GPIO_PIN_RESET);

                us_tick  = HAL_GetTick();
                us_state = US_WAIT_ECHO;
            }
            break;

        case US_WAIT_ECHO:
            /* The capture callback updates echo_us asynchronously.
             * Wait up to 40 ms for the echo, then go back to idle.
             * 40 ms corresponds to ~6.8 m max range — well beyond HC-SR04's 4 m. */
            if (HAL_GetTick() - us_tick >= 40)
            {
                us_state = US_IDLE;
                us_tick  = HAL_GetTick();
            }
            break;

        default:
            us_state = US_IDLE;
            break;
    }
}


/* ===========================================================================
 *  PUBLIC — Distance getters
 * =========================================================================== */

float Ultrasonic_GetDistance_cm(void)
{
    return (echo_us * SPEED_OF_SOUND_CM_PER_US) / 2.0f;
}

float Ultrasonic_GetEcho_us(void)
{
    return echo_us;
}


/* ===========================================================================
 *  PUBLIC — Input capture callback
 * =========================================================================== */

void Ultrasonic_CaptureCallback(TIM_HandleTypeDef *htim)
{
    if (htim->Instance != htim_capture->Instance)
        return;

    if (HAL_GPIO_ReadPin(ECHO_PORT, ECHO_PIN) == GPIO_PIN_SET)
    {
        /* Rising edge — start of echo pulse */
        tc1 = HAL_TIM_ReadCapturedValue(htim, TIM_CHANNEL_1);
    }
    else
    {
        /* Falling edge — end of echo pulse */
        tc2 = HAL_TIM_ReadCapturedValue(htim, TIM_CHANNEL_1);
        if (tc2 > tc1)
            echo_us = (float)(tc2 - tc1);
        else
            echo_us = (float)((65536 - tc1) + tc2);
    }
}