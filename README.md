# hiwonder-servo-sdk

Python / ESP32 / Raspberry Pi SDK for the **Hiwonder HX-30HM** magnetic-encoder bus servo.

- Protocol: UART half-duplex, 1 Mbps, little-endian packet framing  
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
├── python/                  ← Pure-Python SDK (Windows / Linux / macOS)
│   ├── hiwonder_servo.py
│   ├── requirements.txt
│   ├── setup.py
│   └── examples/
│       ├── basic_motion.py
│       ├── sync_control.py
│       └── scan_bus.py
│
├── esp32/                   ← Arduino library for ESP32
│   ├── HiwonderServo.h
│   ├── HiwonderServo.cpp
│   └── examples/
│       ├── basic_motion/basic_motion.ino
│       └── sync_control/sync_control.ino
│
└── raspberrypi/             ← Raspberry Pi driver (extends Python SDK)
    ├── hiwonder_servo_rpi.py
    └── examples/
        ├── basic_motion_rpi.py
        └── sync_control_rpi.py
```

---

## Quick Start

### Python (Windows / Linux)

**Requirements**

```bash
pip install pyserial
```

**Move a single servo**

```python
from python.hiwonder_servo import HiwonderBus, Servo

bus   = HiwonderBus("COM3")          # Windows  (Linux: "/dev/ttyUSB0")
servo = Servo(bus, servo_id=1)

servo.move_to(2048, speed=500)       # center, 500 steps/s
print(servo.full_status())
bus.close()
```

**Synchronously move multiple servos**

```python
from python.hiwonder_servo import HiwonderBus, sync_move

bus = HiwonderBus("COM3")
# (servo_id, position, time_ms, speed)
sync_move(bus, [(1, 1000, 0, 400), (2, 2000, 0, 400), (3, 3000, 0, 400)])
bus.close()
```

**Run the examples**

```bash
python python/examples/basic_motion.py --port COM3 --id 1
python python/examples/sync_control.py --port COM3 --ids 1 2 3 4
python python/examples/scan_bus.py     --port COM3
```

---

### ESP32 (Arduino IDE / PlatformIO)

1. Copy the `esp32/` folder into your Arduino libraries directory  
   (e.g. `~/Documents/Arduino/libraries/HiwonderServo/`).
2. Open an example sketch from `esp32/examples/`.
3. Default wiring:

   | Signal | GPIO |
   |--------|------|
   | Servo bus | GPIO 16 (RX), GPIO 17 (TX via 1 kΩ) |

4. Set baud rate 1 000 000 in the sketch (already the default).

**Minimal sketch**

```cpp
#include <HiwonderServo.h>

HiwonderBus bus(Serial2, 16, 17, 1000000UL);
Servo servo(bus, 1);

void setup() {
    Serial.begin(115200);
    servo.moveTo(2048, 500);
}
void loop() {}
```

---

### Raspberry Pi

**UART setup**

```bash
# Disable Bluetooth to free /dev/ttyAMA0 (RPi 3/4/5)
echo "dtoverlay=disable-bt" | sudo tee -a /boot/config.txt
sudo reboot

# Enable UART
sudo raspi-config   →  Interface Options → Serial → enable
```

**Install**

```bash
pip install pyserial
```

**Run**

```bash
python raspberrypi/examples/basic_motion_rpi.py --port /dev/ttyAMA0 --id 1
```

For RS-485/direction-pin wiring, pass `--dir-pin <BCM_GPIO_NUM>`.

---

## Hardware Wiring

```
Host UART TX ──[ 1 kΩ ]──┬── Servo Bus Data
                          │
Host UART RX ─────────────┘
GND ─────────────────────── Servo GND
Power (11.1 V / 3S LiPo) ── Servo V+
```

For multiple servos, daisy-chain all `Data` and `GND` wires in parallel.

---

## Supported Instructions

| Instruction | Code | Description |
|-------------|------|-------------|
| PING | 0x01 | Query servo status |
| READ DATA | 0x02 | Read register(s) |
| WRITE DATA | 0x03 | Write register(s) |
| REG WRITE | 0x04 | Buffered write (see ACTION) |
| ACTION | 0x05 | Execute all buffered REG WRITEs |
| RESET | 0x06 | Restore factory defaults |
| SYNC READ | 0x82 | Read same register from multiple servos |
| SYNC WRITE | 0x83 | Write same register to multiple servos |

See [`docs/protocol.md`](docs/protocol.md) for full frame format, checksum algorithm, and examples.

---

## Key Register Quick Reference

| Register | Address | Notes |
|----------|---------|-------|
| ID | 0x05 | 1 byte, 0–253 |
| Baud rate | 0x06 | 0=1Mbps (default) |
| Target position | 0x2A | 2 bytes, 0–4095 |
| Run speed | 0x2E | 2 bytes, steps/s |
| NVS lock | 0x37 | 0=unlock, 1=lock |
| Current position | 0x38 | 2 bytes, read-only |
| Current speed | 0x3A | 2 bytes, read-only |
| Voltage | 0x3E | 2 bytes, ×0.1 V |
| Temperature | 0x3F | 1 byte, °C |
| Current (mA) | 0x45 | 2 bytes |

Full register table: [`docs/register_table.md`](docs/register_table.md)

---

## License

MIT
