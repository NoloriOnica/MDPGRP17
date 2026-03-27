/**
 * =============================================================================
 *  manoeuvre.h — Command Queue & Manoeuvre Execution System
 *  Board: WHEELTEC STM32F407VET6
 * =============================================================================
 *
 *  OVERVIEW:
 *    Command queue with UART interface for sequential movement execution.
 *    Includes command acknowledgement protocol.
 *
 *  SUPPORTED COMMANDS:
 *      FORWARD <distance_mm>
 *      BACKWARD <distance_mm>
 *      LEFT <angle_degrees>
 *      RIGHT <angle_degrees>
 *      BACKLEFT <angle_degrees>
 *      BACKRIGHT <angle_degrees>
 *      STOP
 *      PAUSE <time_ms>
 *      APPROACH <distance_cm>    — drive forward/backward until ultrasonic
 *                                  sensor reads the target distance
 *
 *  UART PROTOCOL:
 *    Host sends:   "FORWARD 500\n"
 *    STM replies:  "ACK FORWARD 500.0\n"       (command received)
 *    STM replies:  "DONE FORWARD\n"             (command finished)
 *
 *    Error responses:
 *      "ERR UNKNOWN <text>\n"
 *      "ERR QUEUE_FULL\n"
 *      "ERR TIMEOUT\n"
 *
 *  DEPENDENCIES:
 *    motor_encoder.h, imu_fusion.h, servo.h
 *
 * =============================================================================
 */

#ifndef MANOEUVRE_H
#define MANOEUVRE_H

#include "main.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ===========================================================================
 *  CONFIGURATION
 * =========================================================================== */

#define CMD_QUEUE_SIZE      16
#define UART_RX_BUF_SIZE    64

#define DISTANCE_TOLERANCE_MM   5.0f
#define ANGLE_TOLERANCE_DEG     2.0f

#define MANOEUVRE_TIMEOUT_MS   10000

/* ===========================================================================
 *  COMMAND TYPES
 * =========================================================================== */

typedef enum {
    CMD_NONE = 0,
    CMD_FORWARD,
    CMD_BACKWARD,
    CMD_LEFT,
    CMD_RIGHT,
    CMD_BACK_LEFT,
    CMD_BACK_RIGHT,
    CMD_STOP,
    CMD_PAUSE,
    CMD_APPROACH,
} CmdType_t;

typedef struct {
    CmdType_t type;
    float     param;
} Command_t;

/* ===========================================================================
 *  MANOEUVRE STATE
 * =========================================================================== */

typedef enum {
    MAN_IDLE = 0,
    MAN_RUNNING,
    MAN_COMPLETE,
} ManState_t;

/* ===========================================================================
 *  ULTRASONIC DISTANCE CALLBACK
 * =========================================================================== */

/* Function pointer type: returns current ultrasonic distance in cm */
typedef float (*DistanceGetterFn)(void);

/* ===========================================================================
 *  PUBLIC API
 * =========================================================================== */

void        Manoeuvre_Init(UART_HandleTypeDef *huart);
void        Manoeuvre_SetDistanceCallback(DistanceGetterFn fn);
void        Manoeuvre_Process(void);
bool        Manoeuvre_Enqueue(Command_t cmd);
void        Manoeuvre_EmergencyStop(void);
ManState_t  Manoeuvre_GetState(void);
uint8_t     Manoeuvre_GetQueueCount(void);
void        Manoeuvre_UART_RxCallback(UART_HandleTypeDef *huart);

#ifdef __cplusplus
}
#endif

#endif /* MANOEUVRE_H */