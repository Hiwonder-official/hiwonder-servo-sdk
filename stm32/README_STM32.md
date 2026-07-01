# STM32 HAL Driver — HX-30HM Bus Servo

This driver works with any STM32 series that has the STM32 HAL UART API (`HAL_UART_Transmit` / `HAL_UART_Receive`). Tested configuration: STM32F4 + HAL 1.8.

---

## Files

| File | Description |
|------|-------------|
| `hiwonder_servo_stm32.h` | Register defines, data structures, function declarations |
| `hiwonder_servo_stm32.c` | Full implementation |
| `examples/basic_motion.c` | Single servo — move through five positions |
| `examples/sync_control.c` | Four servos — wave pattern + sync read |

---

## Hardware Wiring

```
STM32 UART TX ──[ 1 kΩ ]──┬── Servo Bus DATA
                            │
STM32 UART RX ─────────────┘
GND ──────────────────────── Servo GND
11.1 V (3S LiPo) ─────────── Servo V+
```

For RS-485 transceiver chips (e.g. MAX485), connect the DE/RE pin to any GPIO output and pass it as `dir_port` / `dir_pin` in `HW_Bus`.

---

## STM32CubeIDE / CubeMX Setup

1. Open your `.ioc` file in CubeMX.

2. Enable the UART peripheral you want to use (e.g. **USART2**):
   - Mode: **Asynchronous**
   - Baud Rate: **1 000 000** (1 Mbps)
   - Word Length: **8 bits**
   - Parity: **None**
   - Stop Bits: **1**

3. If you are using an RS-485 chip with a direction pin, configure one GPIO pin as **GPIO_Output** and give it a user label (e.g. `SERVO_DIR`).

4. Generate code → open in STM32CubeIDE.

5. Copy `hiwonder_servo_stm32.h` and `hiwonder_servo_stm32.c` into your project's `Core/Src` and `Core/Inc` directories (or add them to a library folder in your project tree).

6. In your source file, include the HAL header **before** the driver header:

   ```c
   #include "stm32f4xx_hal.h"    /* replace with your chip family */
   #include "hiwonder_servo_stm32.h"
   ```

   > For other families change the include: `stm32f1xx_hal.h`, `stm32g0xx_hal.h`, `stm32h7xx_hal.h`, etc.

---

## Minimal Usage

```c
#include "stm32f4xx_hal.h"
#include "hiwonder_servo_stm32.h"

extern UART_HandleTypeDef huart2;

HW_Bus bus = {
    .huart    = &huart2,
    .dir_port = NULL,   /* no RS-485 chip */
    .dir_pin  = 0,
    .timeout  = 10,     /* 10 ms per HAL call */
};

/* Ping servo ID 1 */
if (hw_ping(&bus, 1)) {
    /* Move to center at 500 steps/s */
    hw_move_to(&bus, 1, 2048, 0, 500);
    HAL_Delay(1000);

    /* Read position */
    uint16_t pos;
    hw_get_position(&bus, 1, &pos);
}
```

---

## Synchronous Move (4 servos)

```c
HW_MoveCmd cmds[4] = {
    {.id = 1, .position = 1000, .time_ms = 0, .speed = 400},
    {.id = 2, .position = 2000, .time_ms = 0, .speed = 400},
    {.id = 3, .position = 3000, .time_ms = 0, .speed = 400},
    {.id = 4, .position = 2048, .time_ms = 0, .speed = 400},
};
hw_sync_move(&bus, cmds, 4);
```

All four servos start moving simultaneously because `hw_sync_move` sends a single SYNC WRITE packet.

---

## Register Notes

### 0x2A vs 0x28 — which target position register to use?

Always use **0x2A** (via `hw_move_to`). It is part of an atomic 6-byte block:

| Address | Name | Bytes |
|---------|------|-------|
| 0x2A | Target position | 2 |
| 0x2C | Run time (ms) | 2 |
| 0x2E | Run speed (steps/s) | 2 |

One `WRITE DATA` instruction spanning addresses 0x2A–0x2F sets position, time, and speed in a single atomic operation — the servo applies all three values at the same instant.

Using 0x28 + 0x29 requires two separate write commands. Between the two writes the servo may already start moving with stale speed/time values.

---

## NVS (Persistent) Writes

Registers in the ROM area (0x00–0x27) survive power cycles but require unlocking the NVS write-protect register first:

```c
/* hw_set_id() and hw_set_baud() do this automatically */
hw_set_id(&bus, /*current_id=*/1, /*new_id=*/2);
```

If you need to write an NVS register directly, use the pattern:
1. Write `0` to `HW_REG_NVS_LOCK` (0x37) — unlock
2. Write the target register
3. Write `1` to `HW_REG_NVS_LOCK` — re-lock

---

## Troubleshooting

| Symptom | Likely cause |
|---------|-------------|
| `hw_ping` always returns false | Wrong baud rate, TX/RX swapped, missing 1 kΩ resistor, servo not powered |
| Checksum error | Signal integrity — try shorter cable or lower baud via `hw_set_baud` |
| Works for one servo, fails for others | Different servo IDs — run a scan with `hw_ping` in a loop over IDs 0–253 |
| Direction-control pin needed | Using MAX485 or similar RS-485 chip — set `dir_port` / `dir_pin` in `HW_Bus` |
