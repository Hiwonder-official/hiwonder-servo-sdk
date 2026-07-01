/**
 * hiwonder_servo_stm32.c
 * STM32 HAL driver implementation for HX-30HM bus servo
 */

#include "hiwonder_servo_stm32.h"
#include <string.h>

/* -------------------------------------------------------------------------
 * Internal helpers
 * ---------------------------------------------------------------------- */

static uint8_t _checksum(const uint8_t *payload, uint8_t len)
{
    uint32_t sum = 0;
    for (uint8_t i = 0; i < len; i++) {
        sum += payload[i];
    }
    return (uint8_t)(~sum & 0xFFu);
}

/**
 * Build a packet into *buf*.
 * Returns total packet length.
 */
static uint8_t _build_packet(uint8_t *buf,
                              uint8_t servo_id,
                              uint8_t instruction,
                              const uint8_t *params,
                              uint8_t param_len)
{
    uint8_t length = param_len + 2u;  /* instruction + checksum */
    uint8_t idx = 0;
    buf[idx++] = 0xFFu;
    buf[idx++] = 0xFFu;
    buf[idx++] = servo_id;
    buf[idx++] = length;
    buf[idx++] = instruction;
    for (uint8_t i = 0; i < param_len; i++) {
        buf[idx++] = params[i];
    }
    /* checksum covers: id, length, instruction, params */
    buf[idx] = _checksum(&buf[2], 2u + param_len + 1u);
    idx++;
    return idx;
}

static void _dir_tx(HW_Bus *bus)
{
    if (bus->dir_port != NULL) {
        HAL_GPIO_WritePin(bus->dir_port, bus->dir_pin, GPIO_PIN_SET);
    }
}

static void _dir_rx(HW_Bus *bus)
{
    if (bus->dir_port != NULL) {
        HAL_GPIO_WritePin(bus->dir_port, bus->dir_pin, GPIO_PIN_RESET);
    }
}

/**
 * Send a packet, then switch to RX mode.
 */
static HAL_StatusTypeDef _send(HW_Bus *bus, const uint8_t *pkt, uint8_t pkt_len)
{
    _dir_tx(bus);
    HAL_StatusTypeDef st = HAL_UART_Transmit(bus->huart, (uint8_t *)pkt, pkt_len, bus->timeout);
    _dir_rx(bus);
    return st;
}

/**
 * Receive one response packet.
 * *param_len* is the expected number of data bytes in the response.
 * Returns HAL_OK and fills *params* on success.
 */
static HAL_StatusTypeDef _recv(HW_Bus *bus,
                                uint8_t expected_id,
                                uint8_t param_len,
                                uint8_t *params)
{
    /* Read until we see 0xFF 0xFF header */
    uint8_t hdr[2] = {0, 0};
    uint32_t attempts = 0;
    while (attempts++ < 32u) {
        hdr[0] = hdr[1];
        HAL_StatusTypeDef st = HAL_UART_Receive(bus->huart, &hdr[1], 1, bus->timeout);
        if (st != HAL_OK) {
            return HAL_TIMEOUT;
        }
        if (hdr[0] == 0xFFu && hdr[1] == 0xFFu) {
            break;
        }
        if (attempts >= 32u) {
            return HAL_TIMEOUT;
        }
    }

    /* id + length + error + params + checksum */
    uint8_t meta[3];  /* id, length, error */
    if (HAL_UART_Receive(bus->huart, meta, 3, bus->timeout) != HAL_OK) {
        return HAL_TIMEOUT;
    }

    uint8_t length = meta[1];  /* includes instruction/error byte + checksum */
    uint8_t error  = meta[2];
    uint8_t actual_params = length - 2u;  /* minus error byte and checksum */

    if (actual_params > param_len) {
        /* drain excess bytes */
        uint8_t dummy;
        for (uint8_t i = 0; i < actual_params - param_len; i++) {
            HAL_UART_Receive(bus->huart, &dummy, 1, bus->timeout);
        }
        actual_params = param_len;
    }

    if (actual_params > 0 && params != NULL) {
        if (HAL_UART_Receive(bus->huart, params, actual_params, bus->timeout) != HAL_OK) {
            return HAL_TIMEOUT;
        }
    }

    /* read checksum byte */
    uint8_t cs_received;
    if (HAL_UART_Receive(bus->huart, &cs_received, 1, bus->timeout) != HAL_OK) {
        return HAL_TIMEOUT;
    }

    /* verify checksum: covers id + length + error + params */
    uint8_t cs_data[2 + HW_MAX_PACKET_LEN];
    cs_data[0] = meta[0];   /* id */
    cs_data[1] = meta[1];   /* length */
    cs_data[2] = meta[2];   /* error */
    if (actual_params > 0 && params != NULL) {
        memcpy(&cs_data[3], params, actual_params);
    }
    uint8_t cs_calc = _checksum(cs_data, 3u + actual_params);
    if (cs_calc != cs_received) {
        return HAL_ERROR;
    }

    if (error != 0u) {
        return HAL_ERROR;
    }

    (void)expected_id;
    return HAL_OK;
}

/* -------------------------------------------------------------------------
 * Core bus functions
 * ---------------------------------------------------------------------- */

bool hw_ping(HW_Bus *bus, uint8_t servo_id)
{
    uint8_t pkt[HW_MAX_PACKET_LEN];
    uint8_t pkt_len = _build_packet(pkt, servo_id, 0x01u, NULL, 0);
    if (_send(bus, pkt, pkt_len) != HAL_OK) {
        return false;
    }
    return (_recv(bus, servo_id, 0, NULL) == HAL_OK);
}

HAL_StatusTypeDef hw_read(HW_Bus *bus, uint8_t servo_id,
                           uint8_t start_addr, uint8_t length,
                           uint8_t *out)
{
    uint8_t p[2] = {start_addr, length};
    uint8_t pkt[HW_MAX_PACKET_LEN];
    uint8_t pkt_len = _build_packet(pkt, servo_id, 0x02u, p, 2);
    HAL_StatusTypeDef st = _send(bus, pkt, pkt_len);
    if (st != HAL_OK) return st;
    return _recv(bus, servo_id, length, out);
}

HAL_StatusTypeDef hw_write(HW_Bus *bus, uint8_t servo_id,
                            uint8_t start_addr,
                            const uint8_t *data, uint8_t length)
{
    uint8_t params[HW_MAX_PACKET_LEN];
    params[0] = start_addr;
    memcpy(&params[1], data, length);
    uint8_t pkt[HW_MAX_PACKET_LEN];
    uint8_t pkt_len = _build_packet(pkt, servo_id, 0x03u, params, length + 1u);
    HAL_StatusTypeDef st = _send(bus, pkt, pkt_len);
    if (st != HAL_OK) return st;
    if (servo_id != HW_BROADCAST_ID) {
        return _recv(bus, servo_id, 0, NULL);
    }
    return HAL_OK;
}

HAL_StatusTypeDef hw_reg_write(HW_Bus *bus, uint8_t servo_id,
                                uint8_t start_addr,
                                const uint8_t *data, uint8_t length)
{
    uint8_t params[HW_MAX_PACKET_LEN];
    params[0] = start_addr;
    memcpy(&params[1], data, length);
    uint8_t pkt[HW_MAX_PACKET_LEN];
    uint8_t pkt_len = _build_packet(pkt, servo_id, 0x04u, params, length + 1u);
    HAL_StatusTypeDef st = _send(bus, pkt, pkt_len);
    if (st != HAL_OK) return st;
    if (servo_id != HW_BROADCAST_ID) {
        return _recv(bus, servo_id, 0, NULL);
    }
    return HAL_OK;
}

void hw_action(HW_Bus *bus)
{
    uint8_t pkt[HW_MAX_PACKET_LEN];
    uint8_t pkt_len = _build_packet(pkt, HW_BROADCAST_ID, 0x05u, NULL, 0);
    _send(bus, pkt, pkt_len);
}

HAL_StatusTypeDef hw_reset(HW_Bus *bus, uint8_t servo_id)
{
    uint8_t pkt[HW_MAX_PACKET_LEN];
    uint8_t pkt_len = _build_packet(pkt, servo_id, 0x06u, NULL, 0);
    HAL_StatusTypeDef st = _send(bus, pkt, pkt_len);
    if (st != HAL_OK) return st;
    if (servo_id != HW_BROADCAST_ID) {
        return _recv(bus, servo_id, 0, NULL);
    }
    return HAL_OK;
}

/* -------------------------------------------------------------------------
 * High-level servo helpers
 * ---------------------------------------------------------------------- */

HAL_StatusTypeDef hw_move_to(HW_Bus *bus, uint8_t servo_id,
                              uint16_t position,
                              uint16_t time_ms,
                              uint16_t speed_steps_s)
{
    /* Write atomic 6-byte block: TARGET_POS(2) + MOVE_TIME(2) + MOVE_SPEED(2) */
    uint8_t data[6] = {
        (uint8_t)(position     & 0xFFu), (uint8_t)((position     >> 8) & 0xFFu),
        (uint8_t)(time_ms      & 0xFFu), (uint8_t)((time_ms      >> 8) & 0xFFu),
        (uint8_t)(speed_steps_s& 0xFFu), (uint8_t)((speed_steps_s>> 8) & 0xFFu),
    };
    return hw_write(bus, servo_id, HW_REG_TARGET_POS, data, 6);
}

HAL_StatusTypeDef hw_get_position(HW_Bus *bus, uint8_t servo_id, uint16_t *out)
{
    uint8_t buf[2];
    HAL_StatusTypeDef st = hw_read(bus, servo_id, HW_REG_CURRENT_POS, 2, buf);
    if (st == HAL_OK) {
        *out = (uint16_t)(buf[0] | ((uint16_t)buf[1] << 8));
    }
    return st;
}

HAL_StatusTypeDef hw_full_status(HW_Bus *bus, uint8_t servo_id, HW_ServoStatus *out)
{
    /* Read 8 bytes from 0x38: pos(2)+spd(2)+load(2)+volt(2) */
    uint8_t buf[8];
    HAL_StatusTypeDef st = hw_read(bus, servo_id, HW_REG_CURRENT_POS, 8, buf);
    if (st != HAL_OK) return st;

    out->position   = (uint16_t)(buf[0] | ((uint16_t)buf[1] << 8));
    out->speed      = (uint16_t)(buf[2] | ((uint16_t)buf[3] << 8));
    out->load       = (uint16_t)(buf[4] | ((uint16_t)buf[5] << 8));
    uint16_t raw_v  = (uint16_t)(buf[6] | ((uint16_t)buf[7] << 8));
    out->voltage_mv = (uint16_t)(raw_v * 100u);  /* 0.1 V unit → millivolts */

    uint8_t t;
    st = hw_read(bus, servo_id, HW_REG_CURRENT_TEMP, 1, &t);
    if (st == HAL_OK) {
        out->temperature = t;
    }
    return st;
}

/* -------------------------------------------------------------------------
 * NVS helpers
 * ---------------------------------------------------------------------- */

static HAL_StatusTypeDef _nvs_write(HW_Bus *bus, uint8_t servo_id,
                                     uint8_t addr,
                                     const uint8_t *data, uint8_t len)
{
    uint8_t unlock = 0x00u;
    uint8_t lock   = 0x01u;
    HAL_StatusTypeDef st;

    st = hw_write(bus, servo_id, HW_REG_NVS_LOCK, &unlock, 1);
    if (st != HAL_OK) return st;

    st = hw_write(bus, servo_id, addr, data, len);
    if (st != HAL_OK) {
        hw_write(bus, servo_id, HW_REG_NVS_LOCK, &lock, 1);
        return st;
    }

    return hw_write(bus, servo_id, HW_REG_NVS_LOCK, &lock, 1);
}

HAL_StatusTypeDef hw_set_id(HW_Bus *bus, uint8_t current_id, uint8_t new_id)
{
    return _nvs_write(bus, current_id, HW_REG_ID, &new_id, 1);
}

HAL_StatusTypeDef hw_set_baud(HW_Bus *bus, uint8_t servo_id, uint8_t baud_index)
{
    return _nvs_write(bus, servo_id, HW_REG_BAUD_RATE, &baud_index, 1);
}

/* -------------------------------------------------------------------------
 * Sync operations
 * ---------------------------------------------------------------------- */

HAL_StatusTypeDef hw_sync_move(HW_Bus *bus,
                                const HW_MoveCmd *cmds, uint8_t count)
{
    /* SYNC WRITE params: start_addr(1) + data_len(1) + count×(id(1)+6 bytes) */
    uint8_t params[HW_MAX_PACKET_LEN];
    uint8_t idx = 0;
    params[idx++] = HW_REG_TARGET_POS;
    params[idx++] = 6u;
    for (uint8_t i = 0; i < count; i++) {
        params[idx++] = cmds[i].id;
        params[idx++] = (uint8_t)(cmds[i].position      & 0xFFu);
        params[idx++] = (uint8_t)((cmds[i].position      >> 8) & 0xFFu);
        params[idx++] = (uint8_t)(cmds[i].time_ms        & 0xFFu);
        params[idx++] = (uint8_t)((cmds[i].time_ms       >> 8) & 0xFFu);
        params[idx++] = (uint8_t)(cmds[i].speed          & 0xFFu);
        params[idx++] = (uint8_t)((cmds[i].speed         >> 8) & 0xFFu);
    }
    uint8_t pkt[HW_MAX_PACKET_LEN];
    uint8_t pkt_len = _build_packet(pkt, HW_BROADCAST_ID, 0x83u, params, idx);
    return _send(bus, pkt, pkt_len);
}

HAL_StatusTypeDef hw_sync_read(HW_Bus *bus,
                                const uint8_t *servo_ids, uint8_t id_count,
                                uint8_t start_addr, uint8_t data_len,
                                uint8_t *out)
{
    uint8_t params[HW_MAX_PACKET_LEN];
    uint8_t idx = 0;
    params[idx++] = start_addr;
    params[idx++] = data_len;
    for (uint8_t i = 0; i < id_count; i++) {
        params[idx++] = servo_ids[i];
    }
    uint8_t pkt[HW_MAX_PACKET_LEN];
    uint8_t pkt_len = _build_packet(pkt, HW_BROADCAST_ID, 0x82u, params, idx);
    HAL_StatusTypeDef st = _send(bus, pkt, pkt_len);
    if (st != HAL_OK) return st;

    for (uint8_t i = 0; i < id_count; i++) {
        st = _recv(bus, servo_ids[i], data_len, &out[i * data_len]);
        if (st != HAL_OK) return st;
    }
    return HAL_OK;
}
