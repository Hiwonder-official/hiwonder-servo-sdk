/**
 * hiwonder_servo_stm32.h
 * STM32 HAL driver for HX-30HM magnetic-encoder bus servo
 *
 * Requirements:
 *   - Include your chip HAL header before this file:
 *       #include "stm32f4xx_hal.h"   // adjust for your series
 *   - Half-duplex UART:
 *       TX pin → 1 kΩ resistor → servo data bus
 *       RX pin (or same pin if single-wire half-duplex) ← servo data bus
 *   - Optional direction-control GPIO for RS-485 transceiver chips
 *
 * Protocol: UART, 1 Mbps, 8N1, half-duplex, little-endian
 */

#ifndef HIWONDER_SERVO_STM32_H
#define HIWONDER_SERVO_STM32_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>

/* -------------------------------------------------------------------------
 * Register addresses
 * ---------------------------------------------------------------------- */
#define HW_REG_FIRMWARE_MAIN    0x00u
#define HW_REG_FIRMWARE_SUB     0x01u
#define HW_REG_MODEL_L          0x03u
#define HW_REG_MODEL_H          0x04u
#define HW_REG_ID               0x05u
#define HW_REG_BAUD_RATE        0x06u
#define HW_REG_RESPONSE_LEVEL   0x08u
#define HW_REG_TEMP_LIMIT       0x0Du
#define HW_REG_OVERVOLT_LIMIT   0x0Eu  /* 2 bytes, 0.1 V */
#define HW_REG_UNDERVOLT_LIMIT  0x10u  /* 2 bytes, 0.1 V */
#define HW_REG_MAX_TORQUE       0x13u  /* 2 bytes, permille */
#define HW_REG_PROTECT_CTRL     0x14u
#define HW_REG_LED_ALARM        0x15u
#define HW_REG_POS_P            0x16u  /* 2 bytes */
#define HW_REG_POS_D            0x17u  /* 2 bytes */
#define HW_REG_POS_I            0x18u  /* 2 bytes */
#define HW_REG_MIN_STARTUP      0x1Au  /* 2 bytes */
#define HW_REG_CW_DEADZONE      0x1Bu
#define HW_REG_CCW_DEADZONE     0x1Cu
#define HW_REG_CURRENT_LIMIT    0x1Fu  /* 2 bytes, mA */
#define HW_REG_CONTROL_MODE     0x21u  /* 2 bytes */
#define HW_REG_OVERLOAD_TORQUE  0x22u
#define HW_REG_OVERLOAD_TIME    0x23u
#define HW_REG_OVERLOAD_THR     0x24u  /* 2 bytes */
#define HW_REG_SPD_P            0x25u  /* 2 bytes */
#define HW_REG_OVERCUR_TIME     0x26u  /* 2 bytes, ms */
#define HW_REG_SPD_I            0x27u  /* 2 bytes */
/* 0x28/0x29: simple (non-atomic) target pos + time — avoid in production */
#define HW_REG_SIMPLE_POS       0x28u  /* 2 bytes — non-atomic target pos */
#define HW_REG_SIMPLE_TIME      0x29u  /* 2 bytes — run time for 0x28 only */
/* 0x2A/0x2C/0x2E: RECOMMENDED atomic 6-byte block */
#define HW_REG_TARGET_POS       0x2Au  /* 2 bytes — target position (RECOMMENDED) */
#define HW_REG_MOVE_TIME        0x2Cu  /* 2 bytes, ms */
#define HW_REG_MOVE_SPEED       0x2Eu  /* 2 bytes, steps/s */
#define HW_REG_NVS_LOCK         0x37u  /* 0=unlock, 1=lock (default) */
#define HW_REG_CURRENT_POS      0x38u  /* 2 bytes, read-only */
#define HW_REG_CURRENT_SPD      0x3Au  /* 2 bytes, read-only */
#define HW_REG_CURRENT_LOAD     0x3Cu  /* 2 bytes, read-only */
#define HW_REG_CURRENT_VOLT     0x3Eu  /* 2 bytes, read-only, 0.1 V */
#define HW_REG_CURRENT_TEMP     0x3Fu  /* 1 byte,  read-only, °C */
#define HW_REG_REG_FLAG         0x40u
#define HW_REG_STATUS           0x41u
#define HW_REG_MOVING           0x42u  /* 1 = in motion */
#define HW_REG_CURRENT_AMP      0x45u  /* 2 bytes, mA */

#define HW_BROADCAST_ID         0xFEu

/* Max parameters in a single packet (SYNC WRITE for 10 servos × 7 bytes + overhead) */
#define HW_MAX_PACKET_LEN       128u

/* -------------------------------------------------------------------------
 * Data types
 * ---------------------------------------------------------------------- */

/** Full status returned by hw_servo_full_status(). */
typedef struct {
    uint16_t position;   /* 0–4095 steps */
    uint16_t speed;      /* steps/s */
    uint16_t load;       /* permille of rated torque */
    uint16_t voltage_mv; /* millivolts (raw × 100) */
    uint8_t  temperature;/* °C */
} HW_ServoStatus;

/** One servo command entry for hw_sync_move(). */
typedef struct {
    uint8_t  id;
    uint16_t position;   /* 0–4095 */
    uint16_t time_ms;    /* motion duration ms, 0 = fastest */
    uint16_t speed;      /* max speed steps/s, 0 = no limit */
} HW_MoveCmd;

/**
 * Bus handle — fill before calling any hw_ function.
 *
 * Example (STM32F4, USART2):
 *   HW_Bus bus = {
 *     .huart    = &huart2,
 *     .dir_port = GPIOA,
 *     .dir_pin  = GPIO_PIN_1,   // set NULL/0 if not used
 *     .timeout  = 5,            // 5 ms per byte timeout
 *   };
 */
typedef struct {
    UART_HandleTypeDef *huart;    /**< HAL UART handle */
    GPIO_TypeDef       *dir_port; /**< direction-pin GPIO port (NULL = not used) */
    uint16_t            dir_pin;  /**< direction-pin GPIO pin  (0   = not used) */
    uint32_t            timeout;  /**< HAL timeout in ms for each Tx/Rx call */
} HW_Bus;

/* -------------------------------------------------------------------------
 * Core bus functions
 * ---------------------------------------------------------------------- */

/**
 * Send PING and return true if the servo responds.
 */
bool hw_ping(HW_Bus *bus, uint8_t servo_id);

/**
 * Read *length* bytes from *start_addr*.
 * Data is written into *out* (caller must supply buffer of at least *length* bytes).
 * Returns HAL_OK on success.
 */
HAL_StatusTypeDef hw_read(HW_Bus *bus, uint8_t servo_id,
                           uint8_t start_addr, uint8_t length,
                           uint8_t *out);

/**
 * Write *length* bytes from *data* to *start_addr*.
 */
HAL_StatusTypeDef hw_write(HW_Bus *bus, uint8_t servo_id,
                            uint8_t start_addr,
                            const uint8_t *data, uint8_t length);

/**
 * Buffered (REG WRITE) — data latches on next hw_action().
 */
HAL_StatusTypeDef hw_reg_write(HW_Bus *bus, uint8_t servo_id,
                                uint8_t start_addr,
                                const uint8_t *data, uint8_t length);

/**
 * Broadcast ACTION — all servos execute their buffered REG WRITE simultaneously.
 */
void hw_action(HW_Bus *bus);

/**
 * Reset servo to factory defaults.
 */
HAL_StatusTypeDef hw_reset(HW_Bus *bus, uint8_t servo_id);

/* -------------------------------------------------------------------------
 * High-level servo helpers
 * ---------------------------------------------------------------------- */

/**
 * Move *servo_id* to *position* (0–4095).
 * Set speed_steps_s=0 for no speed limit; set time_ms=0 for fastest motion.
 * Uses the atomic 6-byte block at 0x2A (position + time + speed together).
 */
HAL_StatusTypeDef hw_move_to(HW_Bus *bus, uint8_t servo_id,
                              uint16_t position,
                              uint16_t time_ms,
                              uint16_t speed_steps_s);

/**
 * Read current position (0–4095).
 */
HAL_StatusTypeDef hw_get_position(HW_Bus *bus, uint8_t servo_id, uint16_t *out);

/**
 * Read full status in one packet (position, speed, load, voltage, temperature).
 */
HAL_StatusTypeDef hw_full_status(HW_Bus *bus, uint8_t servo_id, HW_ServoStatus *out);

/* -------------------------------------------------------------------------
 * NVS helpers (persist across power cycles)
 * ---------------------------------------------------------------------- */

/**
 * Change servo ID (writes to NVS, auto-unlocks/re-locks).
 */
HAL_StatusTypeDef hw_set_id(HW_Bus *bus, uint8_t current_id, uint8_t new_id);

/**
 * Set baud rate index 0–7 (writes to NVS).
 * 0=1Mbps  1=500k  2=250k  3=128k  4=115200  5=76800  6=57600  7=38400
 */
HAL_StatusTypeDef hw_set_baud(HW_Bus *bus, uint8_t servo_id, uint8_t baud_index);

/* -------------------------------------------------------------------------
 * Sync operations
 * ---------------------------------------------------------------------- */

/**
 * Move multiple servos simultaneously with one SYNC WRITE packet.
 *
 * cmds  : array of HW_MoveCmd entries
 * count : number of entries
 */
HAL_StatusTypeDef hw_sync_move(HW_Bus *bus,
                                const HW_MoveCmd *cmds, uint8_t count);

/**
 * Read the same register from multiple servos (SYNC READ).
 *
 * servo_ids : array of servo IDs
 * id_count  : number of IDs
 * start_addr: register start address
 * data_len  : bytes per servo to read
 * out       : output buffer, must be id_count × data_len bytes
 */
HAL_StatusTypeDef hw_sync_read(HW_Bus *bus,
                                const uint8_t *servo_ids, uint8_t id_count,
                                uint8_t start_addr, uint8_t data_len,
                                uint8_t *out);

#ifdef __cplusplus
}
#endif

#endif /* HIWONDER_SERVO_STM32_H */
