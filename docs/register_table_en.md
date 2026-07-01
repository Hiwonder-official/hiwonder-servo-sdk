# HX-30HM Register Table

> Model: Hiwonder HX-30HM Magnetic-Encoder Bus Servo  
> Firmware version: 2.43 / Register table version: V3.7  
> Byte order: **Little-Endian** — low byte at lower address

**[中文](register_table.md)**

---

## Notes

- **ROM area (NVS)**: persists across power cycles. Before writing, clear the NVS write-protect lock at address `0x37` to `0`; restore it to `1` after writing to protect the data.
- **SRAM area**: volatile, cleared on power loss. No unlock required — read and write freely.
- **Access**: R = read-only, R/W = read-write.
- **Multi-byte registers**: lower address = low byte (LSB), higher address = high byte (MSB).

---

## ROM Area (NVS — persists after power loss)

| Address (hex) | Bytes | Name | Default | Access | Range | Description |
|---------------|-------|------|---------|--------|-------|-------------|
| 0x00 | 1 | Firmware major version | — | R | — | Servo firmware major version number |
| 0x01 | 1 | Firmware minor version | — | R | — | Servo firmware minor version number |
| 0x03 | 1 | Model number low byte | — | R | — | Lower 8 bits of model identifier |
| 0x04 | 1 | Model number high byte | — | R | — | Upper 8 bits of model identifier |
| 0x05 | 1 | ID | 1 | R/W | 0–253 | Unique servo ID; 254 (0xFE) = broadcast |
| 0x06 | 1 | Baud rate | 0 | R/W | 0–7 | See baud rate index table below |
| 0x08 | 1 | Response level | 1 | R/W | 0–2 | 0 = no reply; 1 = reply to read instructions only; 2 = reply to all instructions |
| 0x0D | 1 | Over-temperature threshold (°C) | 70 | R/W | 1–100 | Triggers over-temperature protection; 1 °C resolution |
| 0x0E | 2 | Over-voltage threshold (0.1 V) | 80 | R/W | 0–255 | Supply voltage above this value triggers protection |
| 0x10 | 2 | Under-voltage threshold (0.1 V) | 40 | R/W | 0–255 | Supply voltage below this value triggers protection |
| 0x13 | 2 | Max torque limit (‰) | 1000 | R/W | 0–1000 | 1000 = 100% rated torque |
| 0x14 | 1 | Protection control | — | R/W | bit field | Bits 0–5; see bit field table below |
| 0x15 | 1 | LED alarm conditions | — | R/W | bit field | Bits 0–5; see bit field table below |
| 0x16 | 2 | Position loop P gain | — | R/W | 0–32767 | Proportional term of position PID |
| 0x17 | 2 | Position loop D gain | — | R/W | 0–32767 | Derivative term of position PID |
| 0x18 | 2 | Position loop I gain | — | R/W | 0–32767 | Integral term of position PID |
| 0x1A | 2 | Minimum startup force (‰) | — | R/W | 0–1000 | Minimum output force applied at startup |
| 0x1B | 1 | Clockwise dead zone (steps) | — | R/W | 0–255 | One step = minimum resolution angle |
| 0x1C | 1 | Counter-clockwise dead zone (steps) | — | R/W | 0–255 | One step = minimum resolution angle |
| 0x1F | 2 | Over-current threshold (mA) | — | R/W | 0–6000 | 1 mA resolution, max 6000 mA |
| 0x21 | 2 | Control mode | 0 | R/W | see notes | BIT11 = direction; 0 = position servo; 1 = constant speed; 2 = open-loop PWM |
| 0x22 | 1 | Overload protection max torque (%) | 20 | R/W | 0–100 | Output limit after overload trigger; e.g. 20 = 20% rated torque |
| 0x23 | 1 | Overload protection time (×10 ms) | — | R/W | 0–254 | 254 × 10 ms = 2540 ms |
| 0x24 | 2 | Overload torque threshold (‰) | — | R/W | 0–1000 | Torque above this value counts toward overload protection timer |
| 0x25 | 2 | Speed loop P gain | — | R/W | 0–32767 | Proportional term of speed loop (constant-speed mode) |
| 0x26 | 2 | Over-current protection time (ms) | — | R/W | 0–65535 | Duration above over-current threshold before protection triggers |
| 0x27 | 2 | Speed loop I gain | — | R/W | 0–32767 | Integral term of speed loop (constant-speed / PWM mode) |

---

## SRAM Area (volatile — cleared on power loss)

| Address (hex) | Bytes | Name | Default | Access | Range | Description |
|---------------|-------|------|---------|--------|-------|-------------|
| 0x28 | 2 | Target position — simple write (steps) | — | R/W | 0–4095 | Non-atomic write; sets position only. Servo moves at current/default speed. **Not recommended** — see comparison below |
| 0x29 | 2 | Run time — paired with 0x28 only (ms) | 0 | R/W | 0–65535 | Used only with 0x28; requires two separate writes — servo may start moving between them |
| 0x2A | 2 | Target position — atomic block start (steps) | — | R/W | 0–4095 | **Recommended.** Forms a contiguous 6-byte atomic block with 0x2C/0x2E; all three take effect simultaneously in one write |
| 0x2C | 2 | Run time / PWM parameter (ms) | — | R/W | Position mode: 0–65535 ms; PWM mode: BIT10 = direction | Position mode: motion duration (0 = fastest); open-loop PWM mode: duty-cycle control |
| 0x2E | 2 | Run speed (steps/s) | — | R/W | Position mode: 0–32767; BIT15 = direction | Position mode: max speed limit (0 = no limit); constant-speed mode: target speed |
| 0x37 | 1 | NVS write-protect lock | 1 | R/W | 0 / 1 | 1 = locked (default); 0 = unlocked. Unlock before writing ROM area registers |
| 0x38 | 2 | Current position (steps) | — | R | 0–4095 | Real-time angle feedback; ~0.088° per step |
| 0x3A | 2 | Current speed (steps/s) | — | R | — | Real-time speed feedback |
| 0x3C | 2 | Current load (‰) | — | R | 0–1000 | Current output torque as a fraction of rated torque |
| 0x3E | 2 | Current voltage (0.1 V) | — | R | — | Real-time supply voltage |
| 0x3F | 1 | Current temperature (°C) | — | R | — | Internal servo temperature |
| 0x40 | 1 | Async write flag | 0 | R | 0 / 1 | Set to 1 when REG WRITE is received; cleared to 0 after ACTION executes |
| 0x41 | 1 | Servo status | — | R | bit field | Bits 0–5; see bit field table below |
| 0x42 | 1 | Moving flag | — | R | 0 / 1 | 1 = servo is currently moving |
| 0x45 | 2 | Current (mA) | — | R | 0–6000 | Real-time motor current; 1 mA resolution |

---

## Motion Control Registers: 0x28/0x29 vs 0x2A/0x2C/0x2E

The servo exposes two sets of motion control registers. They look similar but behave very differently.

### Option A: 0x28 + 0x29 (non-atomic — not recommended)

| Address | Name | Bytes |
|---------|------|-------|
| 0x28 | Target position (simple write) | 2 |
| 0x29 | Run time (paired with 0x28) | 2 |

- Position and time require **two separate WRITE DATA commands**.
- Between the two writes, the servo may already start moving with stale parameters (race condition).
- No corresponding speed register — speed cannot be constrained with this approach.
- Only use when simple point-to-point positioning is all that is needed and timing / speed are irrelevant.

### Option B: 0x2A + 0x2C + 0x2E (atomic — recommended)

| Address | Name | Bytes |
|---------|------|-------|
| 0x2A | Target position | 2 |
| 0x2C | Run time (ms) | 2 |
| 0x2E | Run speed (steps/s) | 2 |

- The three registers form a **contiguous 6-byte memory block** (0x2A–0x2F).
- A single `WRITE DATA` instruction spanning all 6 bytes applies position, time, and speed **atomically and simultaneously** — no race condition.
- `move_to()` and `sync_move()` in all SDK variants use this approach.
- **Use this approach for all motion control.**

### Comparison

| | 0x28 + 0x29 | 0x2A + 0x2C + 0x2E |
|---|---|---|
| Write method | Two separate writes | One atomic 6-byte write |
| Race condition risk | Yes | No |
| Speed control | Not supported | Supported (0x2E) |
| Multi-servo sync | Not supported | Supported (SYNC WRITE at 0x2A) |
| Recommendation | Not recommended | **Recommended** |

---

## Baud Rate Index Table

| Index value (0x06) | Baud rate (bps) |
|--------------------|-----------------|
| 0 | **1 000 000** (default) |
| 1 | 500 000 |
| 2 | 250 000 |
| 3 | 128 000 |
| 4 | 115 200 |
| 5 | 76 800 |
| 6 | 57 600 |
| 7 | 38 400 |

---

## Control Modes (address 0x21)

BIT11 is the direction bit (0 = forward, 1 = reverse). The remaining bits select the operating mode:

| Mode value | Description |
|------------|-------------|
| 0 | **Position servo mode** (default) — control via target position (0x2A) |
| 1 | **Constant-speed mode** — set target speed via 0x2E |
| 2 | **Open-loop PWM mode** — set duty cycle / direction via 0x2C |

---

## Protection Control Bits (0x14) and LED Alarm Bits (0x15)

| Bit | Condition |
|-----|-----------|
| Bit0 | Over-voltage protection / alarm |
| Bit1 | Under-voltage protection / alarm |
| Bit2 | Over-temperature protection / alarm |
| Bit3 | Over-current protection / alarm |
| Bit4 | Overload protection / alarm |
| Bit5 | Position limit exceeded protection / alarm |

---

## Servo Status Bits (address 0x41)

| Bit | Meaning |
|-----|---------|
| Bit0 | Over-voltage error |
| Bit1 | Under-voltage error |
| Bit2 | Over-temperature error |
| Bit3 | Over-current error |
| Bit4 | Overload error |
| Bit5 | Instruction error |

---

## Quick Address Reference

| Operation | Start address | Bytes | Notes |
|-----------|---------------|-------|-------|
| Read / write ID | 0x05 | 1 | Requires NVS unlock (0x37 = 0) before writing |
| Read / write baud rate | 0x06 | 1 | Requires NVS unlock before writing |
| Motion control (position + time + speed) | 0x2A | 6 | Write all 6 bytes in one instruction |
| Read current position | 0x38 | 2 | Little-endian, unit = steps |
| Read full status (position + speed + load + voltage) | 0x38 | 8 | One read returns 8 bytes |
| NVS write-protect lock | 0x37 | 1 | 0 = unlock; 1 = lock |

---

## HX-30HM Specifications

| Parameter | Value |
|-----------|-------|
| No-load speed | ~0.732 RPM / steps per second (see firmware) |
| Position resolution | 4096 steps / 360° (~0.088° per step) |
| Rated voltage | 11.1 V (3S LiPo) |
| Maximum current | 6000 mA |
| Maximum effective angle | 360° (multi-turn supported; turn count not saved on power loss) |
| Acceleration unit | steps/s² (setting 10 = 1000 steps/s²) |
| Position control range | 0–4095 (absolute position) |
