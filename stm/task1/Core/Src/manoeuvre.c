/**
 * =============================================================================
 *  manoeuvre.c — Command Queue & Manoeuvre Execution System
 *  Board: WHEELTEC STM32F407VET6
 * =============================================================================
 *
 *  DESIGN:
 *    - NO PID for distance or turn speed — constant PWM until target reached.
 *    - Straight-line correction uses P-only control via the SERVO (not
 *      differential motor PWM). Both motors always run at the same speed.
 *    - Turns: servo is set to full lock, motors drive at constant speed
 *      until gyro says the target angle is reached.
 *    - BACKLEFT / BACKRIGHT: same as LEFT/RIGHT but motors run in reverse.
 *
 *  HEADING SOURCE:
 *    Gyro-only yaw from imu_fusion.h. Yaw is RESET to zero before every
 *    instruction so drift does not accumulate across a sequence.
 *
 *  IMPORTANT:
 *    IMU_Fusion_Init() and IMU_Fusion_Update() must be called externally.
 *    IMU_Fusion_Update() should run every 10 ms (e.g., in the main loop).
 *
 * =============================================================================
 */

#include "manoeuvre.h"
#include "motor_encoder.h"
#include "imu_fusion.h"
#include "servo.h"
#include "stm32f4xx_hal.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

/* ===========================================================================
 *  TUNING CONSTANTS
 * =========================================================================== */

#define WHEEL_DIAMETER_MM       65.0f

/*
 * Empirical distance scale correction:
 *   - If robot under-travels, decrease this (<1.0f)
 *   - If robot over-travels,  increase this (>1.0f)
 */
#define DISTANCE_CAL_FACTOR     0.865f
#define DIST_PER_TICK_MM        ((3.14159265f * WHEEL_DIAMETER_MM / COUNTS_PER_REV) * DISTANCE_CAL_FACTOR)

/* Fixed motor PWM for all movements — tune for your surface/battery */
#define DRIVE_PWM               300
#define TURN_PWM                290

/* Straight-line servo correction gain (P-only).
 * Units: servo command per degree of yaw error.
 * Positive yaw error (drifted right) → negative servo command (steer left).
 * Start with 0.05 and increase if the robot still curves. */
#define STRAIGHT_KP             0.05f

/* Limit how far the servo can correct during straight driving.
 * This prevents the servo from going to full lock on a small drift. */
#define STRAIGHT_SERVO_MAX      0.4f

/* Servo steering trim for driving straight (if robot pulls to one side
 * even with zero yaw error, adjust this) */
#define STRAIGHT_STEER_TRIM     -0.4f

/* Turn calibration (steering lock for left/right turns) */
#define TURN_STEER_LEFT         (-1.00f)
#define TURN_STEER_RIGHT        ( 1.00f)

/* Dead zone: ignore yaw errors below this threshold (degrees).
 * Prevents servo twitching from sensor noise when driving straight. */
#define STRAIGHT_YAW_DEADZONE   1.0f

/* Approach command: tolerance in cm for ultrasonic target distance */
#define APPROACH_TOLERANCE_CM   2.0f

/* Approach command: motor PWM (can be slower for precision) */
#define APPROACH_PWM            285


/* ===========================================================================
 *  PRIVATE — Command queue (circular buffer)
 * =========================================================================== */
static Command_t  cmd_queue[CMD_QUEUE_SIZE];
static uint8_t    queue_head = 0;
static uint8_t    queue_tail = 0;
static uint8_t    queue_count = 0;

/* ===========================================================================
 *  PRIVATE — UART receive state
 * =========================================================================== */
static UART_HandleTypeDef *cmd_huart = NULL;
static uint8_t  rx_byte;
static char     rx_buf[UART_RX_BUF_SIZE];
static uint8_t  rx_index = 0;
static volatile bool rx_line_ready = false;

/* ===========================================================================
 *  PRIVATE — Manoeuvre execution state
 * =========================================================================== */
static ManState_t man_state = MAN_IDLE;
static Command_t  current_cmd = { CMD_NONE, 0.0f };

/* Distance tracking (incremental encoder ticks) */
static int32_t drive_prev_ticks_a = 0;
static int32_t drive_prev_ticks_b = 0;
static float   drive_total_distance = 0.0f;

/* Pause timing */
static uint32_t pause_start_tick = 0;

/* Timeout protection */
static uint32_t manoeuvre_start_tick = 0;

/* Ultrasonic distance callback (set by main via Manoeuvre_SetDistanceCallback) */
static DistanceGetterFn get_distance_cm = NULL;


/* ===========================================================================
 *  PRIVATE — Queue operations
 * =========================================================================== */

static bool Queue_Enqueue(Command_t cmd)
{
    if (queue_count >= CMD_QUEUE_SIZE) return false;
    cmd_queue[queue_tail] = cmd;
    queue_tail = (queue_tail + 1) % CMD_QUEUE_SIZE;
    queue_count++;
    return true;
}

static bool Queue_Dequeue(Command_t *cmd)
{
    if (queue_count == 0) return false;
    *cmd = cmd_queue[queue_head];
    queue_head = (queue_head + 1) % CMD_QUEUE_SIZE;
    queue_count--;
    return true;
}

static void Queue_Clear(void)
{
    queue_head  = 0;
    queue_tail  = 0;
    queue_count = 0;
}


/* ===========================================================================
 *  PRIVATE — Command name helper
 * =========================================================================== */

static const char* CmdName(CmdType_t type)
{
    switch (type)
    {
        case CMD_FORWARD:    return "FORWARD";
        case CMD_BACKWARD:   return "BACKWARD";
        case CMD_LEFT:       return "LEFT";
        case CMD_RIGHT:      return "RIGHT";
        case CMD_BACK_LEFT:  return "BACKLEFT";
        case CMD_BACK_RIGHT: return "BACKRIGHT";
        case CMD_PAUSE:      return "PAUSE";
        case CMD_STOP:       return "STOP";
        case CMD_APPROACH:   return "APPROACH";
        default:             return "NONE";
    }
}


/* ===========================================================================
 *  PRIVATE — Distance tracking update
 * =========================================================================== */

static void Distance_Update(void)
{
    int32_t ticks_a = Encoder_GetCount(MOTOR_A);
    int32_t ticks_b = -Encoder_GetCount(MOTOR_B);

    int32_t delta_a = ticks_a - drive_prev_ticks_a;
    int32_t delta_b = ticks_b - drive_prev_ticks_b;

    drive_prev_ticks_a = ticks_a;
    drive_prev_ticks_b = ticks_b;

    float ds = ((float)delta_a + (float)delta_b) / 2.0f * DIST_PER_TICK_MM;
    drive_total_distance += fabsf(ds);
}


/* ===========================================================================
 *  PRIVATE — Parse UART command string
 * =========================================================================== */

static void Parse_Command(const char *line)
{
    Command_t cmd = { CMD_NONE, 0.0f };
    char cmd_str[16] = {0};

    /* Extract command name (first word) */
    int i = 0;
    while (*line == ' ') line++;  /* Skip leading spaces */
    while (*line && *line != ' ' && i < 15)
    {
        char c = *line++;
        if (c >= 'a' && c <= 'z') c -= 32;  /* Convert to uppercase */
        cmd_str[i++] = c;
    }
    cmd_str[i] = '\0';
    if (i == 0) return;

    /* Extract parameter (float) */
    while (*line == ' ') line++;  /* Skip spaces */
    bool has_param = (*line != '\0');
    float param = 0.0f;
    if (has_param)
    {
        param = strtof(line, NULL);
    }

    if (strcmp(cmd_str, "FORWARD") == 0 && has_param)
    {
        cmd.type  = CMD_FORWARD;
        cmd.param = param;
    }
    else if (strcmp(cmd_str, "BACKWARD") == 0 && has_param)
    {
        cmd.type  = CMD_BACKWARD;
        cmd.param = param;
    }
    else if (strcmp(cmd_str, "LEFT") == 0 && has_param)
    {
        cmd.type  = CMD_LEFT;
        cmd.param = param;
    }
    else if (strcmp(cmd_str, "RIGHT") == 0 && has_param)
    {
        cmd.type  = CMD_RIGHT;
        cmd.param = param;
    }
    else if (strcmp(cmd_str, "BACKLEFT") == 0 && has_param)
    {
        cmd.type  = CMD_BACK_LEFT;
        cmd.param = param;
    }
    else if (strcmp(cmd_str, "BACKRIGHT") == 0 && has_param)
    {
        cmd.type  = CMD_BACK_RIGHT;
        cmd.param = param;
    }
    else if (strcmp(cmd_str, "PAUSE") == 0 && has_param)
    {
        cmd.type  = CMD_PAUSE;
        cmd.param = param;
    }
    else if (strcmp(cmd_str, "APPROACH") == 0 && has_param)
    {
        cmd.type  = CMD_APPROACH;
        cmd.param = param;
    }
    else if (strcmp(cmd_str, "STOP") == 0)
    {
        Manoeuvre_EmergencyStop();
        printf("ACK STOP\r\n");
        return;
    }
    else
    {
        printf("ERR UNKNOWN %s\r\n", cmd_str);
        return;
    }

    if (Queue_Enqueue(cmd))
    {
        printf("ACK %s %.1f\r\n", cmd_str, cmd.param);
    }
    else
    {
        printf("ERR QUEUE_FULL\r\n");
    }
}


/* ===========================================================================
 *  PRIVATE — Manoeuvre completion helper
 *
 *  After every completed instruction, reset the gyro yaw to zero so that
 *  drift from this manoeuvre does not affect the next one.
 * =========================================================================== */

static void Finish_Manoeuvre(const char *status)
{
    MotorA_Brake();
    MotorB_Brake();

    /* Centre servo after every manoeuvre */
    Servo_SetTick(SERVO_TICK_CENTER);

    /* Wait for robot to settle, then reset yaw */
    HAL_Delay(200);
    // IMU_Fusion_ResetYaw();

    if (current_cmd.type == CMD_APPROACH)
        printf("%s %s dist=%.1f\r\n", status, CmdName(current_cmd.type), drive_total_distance);
    else
        printf("%s %s\r\n", status, CmdName(current_cmd.type));

    man_state = MAN_COMPLETE;
}


/* ===========================================================================
 *  PRIVATE — Start manoeuvres
 * =========================================================================== */

static void Start_Drive(Command_t *cmd)
{
    Encoder_Reset(MOTOR_A);
    Encoder_Reset(MOTOR_B);
    drive_prev_ticks_a = 0;
    drive_prev_ticks_b = 0;
    drive_total_distance = 0.0f;

    /* Reset yaw before starting so we measure deviation from zero */
    IMU_Fusion_ResetYaw();

    manoeuvre_start_tick = HAL_GetTick();
    man_state = MAN_RUNNING;
}

static void Start_Turn(Command_t *cmd)
{
    /* Reset yaw before turn so we measure angle turned from zero */
    IMU_Fusion_ResetYaw();

    /* Set servo to full lock in the turning direction */
    if (cmd->type == CMD_LEFT || cmd->type == CMD_BACK_LEFT)
        Servo_SetSteering(TURN_STEER_LEFT);
    else
        Servo_SetSteering(TURN_STEER_RIGHT);

    manoeuvre_start_tick = HAL_GetTick();
    man_state = MAN_RUNNING;
}

static void Start_Pause(Command_t *cmd)
{
    pause_start_tick = HAL_GetTick();
    man_state = MAN_RUNNING;
}

static void Start_Approach(Command_t *cmd)
{
    Encoder_Reset(MOTOR_A);
    Encoder_Reset(MOTOR_B);
    drive_prev_ticks_a = 0;
    drive_prev_ticks_b = 0;
    drive_total_distance = 0.0f;

    /* Reset yaw for straight-line correction */
    IMU_Fusion_ResetYaw();

    manoeuvre_start_tick = HAL_GetTick();
    man_state = MAN_RUNNING;
}


/* ===========================================================================
 *  PRIVATE — Execute manoeuvres
 * =========================================================================== */

static void Execute_Drive(void)
{
    /* Update distance travelled */
    Distance_Update();

    float target_mm = current_cmd.param;
    float distance_error = target_mm - drive_total_distance;

    /* --- Straight-line correction via SERVO (P-only) ---
     *
     * Read current yaw (reset to 0 at drive start).
     * Any non-zero yaw means we've drifted off course.
     * Apply a proportional servo correction to steer back.
     *
     * Sign convention:
     *   yaw > 0 (drifted right/CW) → need to steer left → negative servo
     *   yaw < 0 (drifted left/CCW) → need to steer right → positive servo
     */
    float current_yaw = IMU_Fusion_GetYaw();

    float servo_correction = 0.0f;

    if (fabsf(current_yaw) > STRAIGHT_YAW_DEADZONE)
    {
        /*
         * When driving FORWARD, steering left corrects a rightward drift.
         * When driving BACKWARD, the effect is reversed — steering left
         * pushes the rear right. So we flip the correction sign.
         */
        float sign = (current_cmd.type == CMD_FORWARD) ? -1.0f : 1.0f;
        servo_correction = sign * STRAIGHT_KP * current_yaw;

        /* Clamp correction range */
        if (servo_correction > STRAIGHT_SERVO_MAX)
            servo_correction = STRAIGHT_SERVO_MAX;
        else if (servo_correction < -STRAIGHT_SERVO_MAX)
            servo_correction = -STRAIGHT_SERVO_MAX;
    }

    Servo_SetSteering(STRAIGHT_STEER_TRIM + servo_correction);

    /* Both motors at the same constant speed */
    int16_t motor_speed;
    if (current_cmd.type == CMD_FORWARD)
        motor_speed = DRIVE_PWM;
    else
        motor_speed = -DRIVE_PWM;

    Motor_SetSpeed(MOTOR_A, motor_speed);
    Motor_SetSpeed(MOTOR_B, motor_speed);

    /* Check completion */
    if (distance_error <= DISTANCE_TOLERANCE_MM)
    {
        Finish_Manoeuvre("DONE");
        return;
    }

    /* Timeout */
    if (HAL_GetTick() - manoeuvre_start_tick > MANOEUVRE_TIMEOUT_MS)
    {
        Finish_Manoeuvre("ERR TIMEOUT");
    }
}

static void Execute_Turn(void)
{
    float current_yaw = IMU_Fusion_GetYaw();
    float abs_turned = fabsf(current_yaw);
    float target_angle = current_cmd.param;
    float angle_error = target_angle - abs_turned;

    /* Constant speed — forward for LEFT/RIGHT, reverse for BACKLEFT/BACKRIGHT */
    int16_t motor_speed;
    if (current_cmd.type == CMD_BACK_LEFT || current_cmd.type == CMD_BACK_RIGHT)
        motor_speed = -TURN_PWM;
    else
        motor_speed = TURN_PWM;

    Motor_SetSpeed(MOTOR_A, motor_speed);
    Motor_SetSpeed(MOTOR_B, motor_speed);

    /* Check completion */
    if (angle_error <= ANGLE_TOLERANCE_DEG)
    {
        Finish_Manoeuvre("DONE");
        return;
    }

    /* Timeout */
    if (HAL_GetTick() - manoeuvre_start_tick > MANOEUVRE_TIMEOUT_MS)
    {
        Finish_Manoeuvre("ERR TIMEOUT");
    }
}

static void Execute_Pause(void)
{
    if (HAL_GetTick() - pause_start_tick >= (uint32_t)current_cmd.param)
    {
        Finish_Manoeuvre("DONE");
    }
}

static void Execute_Approach(void)
{
    /* Track distance travelled */
    Distance_Update();

    /* If no distance callback registered, abort immediately */
    if (get_distance_cm == NULL)
    {
        Finish_Manoeuvre("ERR NO_SENSOR");
        return;
    }

    float current_cm = get_distance_cm();
    float target_cm  = current_cmd.param;
    float error_cm   = current_cm - target_cm;

    /* Check completion — within tolerance */
    if (fabsf(error_cm) <= APPROACH_TOLERANCE_CM)
    {
        Finish_Manoeuvre("DONE");
        return;
    }

    /* Straight-line correction via servo (same as Execute_Drive) */
    float current_yaw = IMU_Fusion_GetYaw();
    float servo_correction = 0.0f;

    if (fabsf(current_yaw) > STRAIGHT_YAW_DEADZONE)
    {
        /* Direction depends on whether we're driving forward or backward */
        float sign = (error_cm > 0.0f) ? -1.0f : 1.0f;
        servo_correction = sign * STRAIGHT_KP * current_yaw;

        if (servo_correction > STRAIGHT_SERVO_MAX)
            servo_correction = STRAIGHT_SERVO_MAX;
        else if (servo_correction < -STRAIGHT_SERVO_MAX)
            servo_correction = -STRAIGHT_SERVO_MAX;
    }

    Servo_SetSteering(STRAIGHT_STEER_TRIM + servo_correction);

    /* Drive direction: positive error = too far away = move forward,
     * negative error = too close = move backward */
    int16_t motor_speed;
    if (error_cm > 0.0f)
        motor_speed = APPROACH_PWM;
    else
        motor_speed = -APPROACH_PWM;

    Motor_SetSpeed(MOTOR_A, motor_speed);
    Motor_SetSpeed(MOTOR_B, motor_speed);

    /* Timeout */
    if (HAL_GetTick() - manoeuvre_start_tick > MANOEUVRE_TIMEOUT_MS)
    {
        Finish_Manoeuvre("ERR TIMEOUT");
    }
}


/* ===========================================================================
 *  PUBLIC — Initialisation
 * =========================================================================== */

void Manoeuvre_Init(UART_HandleTypeDef *huart)
{
    cmd_huart = huart;

    Queue_Clear();
    man_state = MAN_IDLE;

    /*
     * NOTE: IMU_Fusion_Init() must be called BEFORE Manoeuvre_Init().
     */

    /* Start UART receive */
    if (cmd_huart != NULL)
    {
        HAL_UART_Receive_IT(cmd_huart, &rx_byte, 1);
    }
}

void Manoeuvre_SetDistanceCallback(DistanceGetterFn fn)
{
    get_distance_cm = fn;
}


/* ===========================================================================
 *  PUBLIC — Main process loop
 * =========================================================================== */

void Manoeuvre_Process(void)
{
    if (rx_line_ready)
    {
        rx_line_ready = false;
        Parse_Command(rx_buf);
    }

    switch (man_state)
    {
        case MAN_IDLE:
            if (Queue_Dequeue(&current_cmd))
            {
                switch (current_cmd.type)
                {
                    case CMD_FORWARD:
                    case CMD_BACKWARD:
                        Start_Drive(&current_cmd);
                        break;
                    case CMD_LEFT:
                    case CMD_RIGHT:
                    case CMD_BACK_LEFT:
                    case CMD_BACK_RIGHT:
                        Start_Turn(&current_cmd);
                        break;
                    case CMD_PAUSE:
                        Start_Pause(&current_cmd);
                        break;
                    case CMD_APPROACH:
                        Start_Approach(&current_cmd);
                        break;
                    default:
                        break;
                }
            }
            break;

        case MAN_RUNNING:
            switch (current_cmd.type)
            {
                case CMD_FORWARD:
                case CMD_BACKWARD:
                    Execute_Drive();
                    break;
                case CMD_LEFT:
                case CMD_RIGHT:
                case CMD_BACK_LEFT:
                case CMD_BACK_RIGHT:
                    Execute_Turn();
                    break;
                case CMD_PAUSE:
                    Execute_Pause();
                    break;
                case CMD_APPROACH:
                    Execute_Approach();
                    break;
                default:
                    man_state = MAN_COMPLETE;
                    break;
            }
            break;

        case MAN_COMPLETE:
            current_cmd.type = CMD_NONE;
            man_state = MAN_IDLE;
            break;
    }
}


/* ===========================================================================
 *  PUBLIC — Queue access and state
 * =========================================================================== */

bool Manoeuvre_Enqueue(Command_t cmd)
{
    return Queue_Enqueue(cmd);
}

void Manoeuvre_EmergencyStop(void)
{
    Motor_SetSpeed(MOTOR_A, 0);
    Motor_SetSpeed(MOTOR_B, 0);
    Servo_SetSteering(0.0f);

    IMU_Fusion_ResetYaw();

    Queue_Clear();
    current_cmd.type = CMD_NONE;
    man_state = MAN_IDLE;
}

ManState_t Manoeuvre_GetState(void)
{
    return man_state;
}

uint8_t Manoeuvre_GetQueueCount(void)
{
    return queue_count;
}


/* ===========================================================================
 *  PUBLIC — UART receive callback
 * =========================================================================== */

void Manoeuvre_UART_RxCallback(UART_HandleTypeDef *huart)
{
    if (huart != cmd_huart) return;

    if (rx_byte == '\n' || rx_byte == '\r')
    {
        if (rx_index > 0)
        {
            rx_buf[rx_index] = '\0';
            rx_line_ready = true;
            rx_index = 0;
        }
    }
    else
    {
        if (rx_index < UART_RX_BUF_SIZE - 1)
        {
            rx_buf[rx_index++] = (char)rx_byte;
        }
    }

    if (cmd_huart != NULL)
    {
        HAL_UART_Receive_IT(cmd_huart, &rx_byte, 1);
    }
}