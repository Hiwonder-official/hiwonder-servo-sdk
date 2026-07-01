# hiwonder-servo-sdk

Python / ESP32 / Raspberry Pi / STM32 SDK for the **Hiwonder HX-30HM** magnetic-encoder bus servo.

- Protocol: UART half-duplex, 1 Mbps, 8N1, little-endian packet framing
- Position: 12-bit (0–4095 steps, 360° range, ~0.088°/step)
- Multi-servo sync read/write on a single bus wire

---

## Repository Layout

```
hiwonder-servo-sdk/
├── docs/
│   ├── protocol.md          ← Communication protocol (frames, instructions, timing)
│   └── register_table.md   ← Full register map (ROM + SRAM, with units & notes)
│
├── python/                  ← Pure-Python SDK (pip installable)
│   ├── hiwonder/
│   │   ├── __init__.py
│   │   └── servo.py
│   ├── hiwonder_servo.py    ← Backward-compat shim (from hiwonder import *)
│   ├── pyproject.toml
│   ├── setup.py
│   ├── requirements.txt
│   └── examples/
│       ├── basic_motion.py
│       ├── sync_control.py
│       └── scan_bus.py
│
├── esp32/                   ← Arduino library for ESP32
│   ├── library.properties   ← Arduino Library Manager metadata
│   ├── HiwonderServo.h
│   ├── HiwonderServo.cpp
│   └── examples/
│       ├── basic_motion/basic_motion.ino
│       └── sync_control/sync_control.ino
│
├── raspberrypi/             ← Raspberry Pi driver (extends Python SDK)
│   ├── hiwonder_servo_rpi.py
│   └── examples/
│       ├── basic_motion_rpi.py
│       └── sync_control_rpi.py
│
└── stm32/                   ← STM32 HAL C library
    ├── hiwonder_servo_stm32.h
    ├── hiwonder_servo_stm32.c
    ├── README_STM32.md
    └── examples/
        ├── basic_motion.c
        └── sync_control.c
```

---

## Hardware Wiring

The HX-30HM uses a **single-wire half-duplex UART bus**. All servos share the same DATA line.

```
Host UART TX ──[ 1 kΩ ]──┬── Servo Bus DATA
                           │
Host UART RX ──────────────┘
GND ──────────────────────── Servo GND
11.1 V (3S LiPo) ─────────── Servo V+
```

For multiple servos, daisy-chain all `DATA` and `GND` wires in parallel. The 1 kΩ resistor prevents TX/RX bus contention on the host side.

For RS-485 transceiver chips (MAX485 or similar), connect DE/RE to a GPIO output pin and use the direction-pin feature in the relevant SDK.

---

## Python SDK

### Requirements

- Python 3.8+
- pyserial

### Installation

**Option A — Install from source (recommended for development)**

```bash
cd python
pip install -e .
```

**Option B — Install requirements only**

```bash
pip install pyserial
```

After `pip install -e .`, the `hiwonder` package is importable from anywhere:

```python
from hiwonder import HiwonderBus, Servo, sync_move
```

### UART Setup

**Windows** — identify the COM port in Device Manager after connecting the USB-UART adapter. Common ports: `COM3`, `COM4`, etc.

**Linux** — `ls /dev/ttyUSB*` or `ls /dev/ttyACM*` after plugging in.

Set the baud rate to **1 000 000** (1 Mbps). Most USB-UART adapters support this — CH340, CP2102, FT232 all work.

### Move a Single Servo

```python
from hiwonder import HiwonderBus, Servo

bus   = HiwonderBus("COM3")         # Windows  (Linux: "/dev/ttyUSB0")
servo = Servo(bus, servo_id=1)

servo.move_to(2048, speed=500)      # center position, 500 steps/s
print(servo.full_status())
bus.close()
```

### Read Servo State

```python
print(servo.position)       # current position 0–4095
print(servo.temperature)    # °C
print(servo.voltage)        # volts (float)
print(servo.current)        # mA
print(servo.is_moving)      # True / False
```

### Synchronously Move Multiple Servos

```python
from hiwonder import HiwonderBus, sync_move

bus = HiwonderBus("COM3")
# (servo_id, position, time_ms, speed)
sync_move(bus, [
    (1, 1000, 0, 400),
    (2, 2000, 0, 400),
    (3, 3000, 0, 400),
])
bus.close()
```

`sync_move` sends a single SYNC WRITE packet so all servos start simultaneously.

### Change Servo ID

```python
# Servo is currently ID 1; rename it to ID 5
servo.set_id(5)
```

This writes to NVS (persists after power cycle). The change takes effect immediately.

### Run the Examples

```bash
# Single servo
python python/examples/basic_motion.py --port COM3 --id 1

# Four servos in sync
python python/examples/sync_control.py --port COM3 --ids 1 2 3 4

# Scan all IDs on the bus
python python/examples/scan_bus.py --port COM3
```

### Backward Compatibility

Code that uses the old `hiwonder_servo.py` module directly continues to work — `python/hiwonder_servo.py` is now a shim that re-exports everything from the `hiwonder` package.

---

## ESP32 (Arduino IDE / PlatformIO)

### Installation — Arduino IDE

1. In Arduino IDE: **Sketch → Include Library → Add .ZIP Library…**  
   Select the `esp32/` folder (or a ZIP of it).

   Alternatively, copy the entire `esp32/` folder into your Arduino libraries directory:
   - Windows: `C:\Users\<you>\Documents\Arduino\libraries\HiwonderServo\`
   - macOS / Linux: `~/Arduino/libraries/HiwonderServo/`

2. Restart Arduino IDE. The library appears under **Sketch → Include Library → HiwonderServo**.

3. Open an example: **File → Examples → HiwonderServo → basic_motion** or **sync_control**.

### Installation — PlatformIO

Add to `platformio.ini`:

```ini
lib_deps =
    https://github.com/Hiwonder-official/-hiwonder-servo-sdk.git
```

Or copy the `esp32/` folder into your project's `lib/` directory.

### Wiring (ESP32)

Default pin assignment in the library:

| Signal | GPIO |
|--------|------|
| UART RX (receive from bus) | GPIO 16 |
| UART TX → 1 kΩ → bus | GPIO 17 |

Change the pins in the `HiwonderBus` constructor if needed.

### Minimal Sketch

```cpp
#include <HiwonderServo.h>

HiwonderBus bus(Serial2, /*rx=*/16, /*tx=*/17, /*baud=*/1000000UL);
Servo servo(bus, /*id=*/1);

void setup() {
    Serial.begin(115200);

    servo.moveTo(2048, /*speed=*/500);   // center at 500 steps/s
    delay(1500);

    ServoStatus st = servo.fullStatus();
    Serial.printf("pos=%d  volt=%.1fV  temp=%d°C\n",
                  st.position, st.voltage, st.temperature);
}

void loop() {}
```

### Sync Move (ESP32)

```cpp
uint8_t  ids[4]     = {1, 2, 3, 4};
uint16_t positions[4] = {1000, 2000, 3000, 2048};
uint16_t speeds[4]  = {400, 400, 400, 400};
uint16_t times[4]   = {0, 0, 0, 0};

syncMove(bus, ids, positions, speeds, times, 4);
```

---

## Raspberry Pi

### UART Setup

**RPi 3 / 4 / 5** — the default UART (`/dev/ttyAMA0`) is used by Bluetooth. Disable Bluetooth to free it:

```bash
# Append to /boot/config.txt (or /boot/firmware/config.txt on RPi 5)
echo "dtoverlay=disable-bt" | sudo tee -a /boot/config.txt
sudo reboot
```

Then enable the serial port:

```bash
sudo raspi-config
# Interface Options → Serial Port
# "Would you like a login shell accessible over serial?" → No
# "Would you like the serial port hardware to be enabled?" → Yes
```

After reboot, `/dev/ttyAMA0` is available at 1 Mbps.

Alternatively, use a USB-UART adapter on `/dev/ttyUSB0` — no configuration needed.

### Installation

The Raspberry Pi driver reuses the Python SDK. Install the package once:

```bash
cd python
pip install -e .
```

### Run the Examples

```bash
python raspberrypi/examples/basic_motion_rpi.py --port /dev/ttyAMA0 --id 1

python raspberrypi/examples/sync_control_rpi.py --port /dev/ttyAMA0 --ids 1 2 3 4
```

### RS-485 Direction Pin (optional)

If you are using an RS-485 transceiver (MAX485 or SN75176) rather than the direct 1 kΩ resistor wiring, pass the BCM GPIO number for the direction-control pin:

```bash
python raspberrypi/examples/basic_motion_rpi.py --port /dev/ttyAMA0 --id 1 --dir-pin 18
```

In code:

```python
from raspberrypi.hiwonder_servo_rpi import RpiServoBus, Servo

bus   = RpiServoBus("/dev/ttyAMA0", dir_pin=18)   # BCM GPIO 18
servo = Servo(bus, servo_id=1)
servo.move_to(2048, speed=400)
bus.close()
```

---

## STM32

See [`stm32/README_STM32.md`](stm32/README_STM32.md) for full CubeIDE setup, wiring, and API reference.

**Quick start:**

1. Copy `stm32/hiwonder_servo_stm32.h` and `stm32/hiwonder_servo_stm32.c` into your project.
2. Configure a UART peripheral at **1 000 000 baud, 8N1** in CubeMX.
3. Include the HAL header for your chip series **before** the driver header:

   ```c
   #include "stm32f4xx_hal.h"    /* adjust for your series */
   #include "hiwonder_servo_stm32.h"
   ```

4. Create a bus handle and call `hw_move_to`:

   ```c
   HW_Bus bus = { .huart = &huart2, .dir_port = NULL, .timeout = 10 };
   hw_move_to(&bus, /*id=*/1, /*position=*/2048, /*time_ms=*/0, /*speed=*/500);
   ```

---

## Supported Instructions

| Instruction | Code | Description |
|-------------|------|-------------|
| PING | 0x01 | Query servo presence |
| READ DATA | 0x02 | Read register(s) |
| WRITE DATA | 0x03 | Write register(s) |
| REG WRITE | 0x04 | Buffered write (latches on ACTION) |
| ACTION | 0x05 | Execute all buffered REG WRITEs simultaneously |
| RESET | 0x06 | Restore factory defaults |
| SYNC READ | 0x82 | Read same register from multiple servos |
| SYNC WRITE | 0x83 | Write same register to multiple servos |

See [`docs/protocol.md`](docs/protocol.md) for full frame format, checksum algorithm, and examples.

---

## Key Registers

| Register | Address | Notes |
|----------|---------|-------|
| ID | 0x05 | 1 byte, 0–253; write requires NVS unlock |
| Baud rate | 0x06 | 0=1Mbps (default); write requires NVS unlock |
| Target position | 0x2A | 2 bytes, 0–4095; part of atomic 6-byte block |
| Run time | 0x2C | 2 bytes, ms |
| Run speed | 0x2E | 2 bytes, steps/s |
| NVS lock | 0x37 | 0=unlock, 1=lock (default) |
| Current position | 0x38 | 2 bytes, read-only |
| Current speed | 0x3A | 2 bytes, read-only |
| Voltage | 0x3E | 2 bytes, ×0.1 V, read-only |
| Temperature | 0x3F | 1 byte, °C, read-only |
| Current (mA) | 0x45 | 2 bytes, read-only |

> **0x2A vs 0x28**: Always use 0x2A (TARGET_POS). Together with 0x2C (time) and 0x2E (speed), it forms a 6-byte atomic block — one write instruction sets all three simultaneously. Writing 0x28 and 0x29 separately risks a race condition where the servo starts moving between the two writes. See [`docs/register_table.md`](docs/register_table.md) for the full comparison.

Full register table: [`docs/register_table.md`](docs/register_table.md)

---

## FAQ

**Q: The servo doesn't respond at all.**  
A: Check power first (11.1 V / 3S LiPo required). Verify the baud rate is exactly 1 000 000. Swap TX and RX if unsure. Make sure the 1 kΩ resistor is in series with TX, not RX.

**Q: I get checksum errors occasionally.**  
A: Usually a signal integrity issue — try a shorter cable, add a 100 nF decoupling cap near the servo connector, or lower the baud rate with `servo.set_baud_rate(1)` (500 kbps).

**Q: Multiple servos don't all respond.**  
A: All servos ship with ID 1. Connect them one at a time and change each ID with `servo.set_id(n)` before daisy-chaining.

**Q: `pip install -e .` fails with "No module named hiwonder".**  
A: Run the command from the `python/` directory, not the repo root.

**Q: RPi UART is not available / permission denied.**  
A: Run `sudo raspi-config` → Interface Options → Serial Port → disable login shell, enable hardware. Also check `ls -l /dev/ttyAMA0` — the user must be in the `dialout` group (`sudo usermod -aG dialout $USER`).

**Q: ESP32 sketch compiles but the servo doesn't move.**  
A: Confirm `Serial2` is mapped to GPIO 16 (RX) and 17 (TX) on your board variant. Some ESP32-S3 boards map Serial2 differently — check the board pinout and adjust in the `HiwonderBus` constructor.

---

## License

MIT
