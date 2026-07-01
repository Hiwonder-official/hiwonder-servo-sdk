/**
 * sync_control.c — Simultaneously move four HX-30HM servos on STM32.
 *
 * All four servos share the same DATA bus wire.
 * IDs are assumed to be 1, 2, 3, 4 (change HW_IDS as needed).
 *
 * Setup: same as basic_motion.c — USART2 at 1 Mbps, 8N1.
 */

#include "main.h"
#include "hiwonder_servo_stm32.h"

extern UART_HandleTypeDef huart2;

#define HW_SERVO_COUNT  4u

static const uint8_t HW_IDS[HW_SERVO_COUNT] = {1, 2, 3, 4};

int main(void)
{
    HAL_Init();
    SystemClock_Config();
    MX_GPIO_Init();
    MX_USART2_UART_Init();

    HW_Bus bus = {
        .huart    = &huart2,
        .dir_port = NULL,
        .dir_pin  = 0,
        .timeout  = 10,
    };

    /* Ping all servos */
    for (uint8_t i = 0; i < HW_SERVO_COUNT; i++) {
        bool ok = hw_ping(&bus, HW_IDS[i]);
        (void)ok;  /* check ok in debugger or send over UART */
    }

    /* Center all servos */
    HW_MoveCmd center[HW_SERVO_COUNT];
    for (uint8_t i = 0; i < HW_SERVO_COUNT; i++) {
        center[i].id       = HW_IDS[i];
        center[i].position = 2048;
        center[i].time_ms  = 0;
        center[i].speed    = 500;
    }
    hw_sync_move(&bus, center, HW_SERVO_COUNT);
    HAL_Delay(1000);

    /* Wave pattern — 6 steps */
    for (uint8_t step = 0; step < 6; step++) {
        HW_MoveCmd wave[HW_SERVO_COUNT];
        for (uint8_t i = 0; i < HW_SERVO_COUNT; i++) {
            wave[i].id       = HW_IDS[i];
            wave[i].position = (uint16_t)((500u + step * 600u + i * 200u) % 4096u);
            wave[i].time_ms  = 0;
            wave[i].speed    = 400;
        }
        hw_sync_move(&bus, wave, HW_SERVO_COUNT);
        HAL_Delay(800);
    }

    /* Sync-read current positions */
    uint8_t pos_buf[HW_SERVO_COUNT * 2];
    hw_sync_read(&bus, HW_IDS, HW_SERVO_COUNT, HW_REG_CURRENT_POS, 2, pos_buf);
    for (uint8_t i = 0; i < HW_SERVO_COUNT; i++) {
        uint16_t pos = (uint16_t)(pos_buf[i * 2] | ((uint16_t)pos_buf[i * 2 + 1] << 8));
        (void)pos;  /* use pos as needed */
    }

    while (1) {}
}
