# HX-30HM Bus Servo Communication Protocol

> Model: Hiwonder HX-30HM Magnetic-Encoder Bus Servo  
> Protocol version: 1.0 (2025-09-25)  
> Firmware version: 2.43

**[中文](protocol.md)**

---

## 1. Overview

The HX-30HM uses a 32-bit ARM MCU as its main controller. Position sensing is based on a 360° 12-bit magnetic encoder.

The bus topology allows multiple servos to be connected in parallel. Each servo has a unique **ID (0–253)**.  
**ID 254 (0xFE)** is the broadcast ID — all servos receive the packet, but only PING generates a response.

### UART Parameters

| Parameter | Value |
|-----------|-------|
| Interface | UART (single-wire half-duplex) |
| Default baud rate | **1 000 000 bps** |
| Optional baud rates | 500 000 / 250 000 / 128 000 / 115 200 / 76 800 / 57 600 / 38 400 |
| Data bits | 8 |
| Stop bits | 1 |
| Parity | None |
| Frame length | 10 bits (1 start + 8 data + 1 stop) |

---

## 2. Packet Format

### 2.1 Instruction Packet (host → servo)

```
┌────────┬────────┬────────┬─────────────┬─────────────────────────┬──────────┐
│ 0xFF   │ 0xFF   │  ID    │   Length    │  Instruction + Params   │ CheckSum │
└────────┴────────┴────────┴─────────────┴─────────────────────────┴──────────┘
```

| Field | Bytes | Description |
|-------|-------|-------------|
| Header | 2 | Fixed `0xFF 0xFF`, marks the start of a packet |
| ID | 1 | Target servo ID (0x00–0xFD; 0xFE = broadcast) |
| Length | 1 | `N + 2`, where N is the number of parameter bytes |
| Instruction | 1 | Instruction code, see Section 3 |
| Parameter 1…N | N | Instruction-specific parameters |
| CheckSum | 1 | `~(ID + Length + Instruction + P1 + … + PN) & 0xFF` |

> **Checksum**: sum all bytes inside the parentheses, keep only the lowest byte, then invert all bits.

### 2.2 Response Packet (servo → host)

```
┌────────┬────────┬────────┬────────────┬────────┬─────────────────┬──────────┐
│ 0xFF   │ 0xFF   │  ID    │   Length   │ ERROR  │  Parameter 1…N  │ CheckSum │
└────────┴────────┴────────┴────────────┴────────┴─────────────────┴──────────┘
```

| Field | Description |
|-------|-------------|
| ERROR | Current servo error flags (0 = OK) |
| Parameter | Return data, present only for read instructions |

### 2.3 Byte Order

All **multi-byte values are little-endian** — the low byte comes first, the high byte follows.

---

## 3. Instruction Set

| Instruction | Code | Param bytes | Description |
|-------------|------|-------------|-------------|
| PING | `0x01` | 0 | Query servo presence / status |
| READ DATA | `0x02` | 2 | Read bytes from the register table |
| WRITE DATA | `0x03` | ≥ 1 | Write bytes to the register table |
| REG WRITE | `0x04` | ≥ 2 | Buffered write — executes on ACTION |
| ACTION | `0x05` | 0 | Execute all buffered REG WRITEs |
| RESET | `0x06` | 0 | Restore factory defaults |
| SYNC READ | `0x82` | ≥ 3 | Read the same register from multiple servos |
| SYNC WRITE | `0x83` | ≥ 2 | Write the same register to multiple servos |

---

## 4. Instruction Reference

### 4.1 PING — Query Status

**Packet format**

```
FF FF  <ID>  02  01  <CheckSum>
```

**Example** — query servo ID 1

```
Send:   FF FF 01 02 01 FB
Return: FF FF 01 02 00 FC
```

ERROR = 0x00 means the servo is operating normally.

---

### 4.2 READ DATA — Read Registers

**Packet format**

```
FF FF  <ID>  04  02  <StartAddr>  <DataLen>  <CheckSum>
```

| Parameter | Description |
|-----------|-------------|
| StartAddr | Starting register address (1 byte) |
| DataLen | Number of bytes to read (1 byte) |

**Example** — read current position of servo ID 1 (address 0x38, 2 bytes)

```
Send:   FF FF 01 04 02 38 02 BE
Return: FF FF 01 04 00 18 05 DD
```

Return data: low byte = 0x18, high byte = 0x05 → position = 0x0518 = **1304**

---

### 4.3 WRITE DATA — Write Registers

**Packet format**

```
FF FF  <ID>  <N+3>  03  <StartAddr>  <Data1> … <DataN>  <CheckSum>
```

**Example 1** — set servo ID to 1 via broadcast (address 0x05)

```
FF FF FE 04 03 05 01 F4
```

> NVS-area registers require the write-protect lock (address 0x37) to be cleared to 0 before writing.

**Example 2** — move servo ID 1 to position 2048 at speed 1000 steps/s

Write 6 bytes starting at 0x2A: position (2 bytes) + time (2 bytes) + speed (2 bytes)

```
FF FF 01 09 03 2A 00 08 00 00 E8 03 D5
```

Response: `FF FF 01 02 00 FC` (ERROR = 0, command accepted)

---

### 4.4 REG WRITE — Buffered Write

Same format as WRITE DATA, but with instruction code `0x04`.

On receipt, the servo stores the data in a buffer and sets the `Registered Instruction` flag to 1. The data is not applied until an ACTION packet is received.

**Use case**: in multi-servo setups, send REG WRITE to each servo in turn, then send a single ACTION broadcast to make all servos move simultaneously.

---

### 4.5 ACTION — Execute Buffered Write

```
FF FF FE 02 05 FA
```

Uses the broadcast ID (0xFE) — no response is returned. All servos on the bus execute their buffered REG WRITE commands at the same time.

---

### 4.6 RESET — Restore Factory Defaults

Resets the register table to factory values.

```
FF FF <ID> 02 06 <CheckSum>
```

**Example** — reset servo ID 1

```
Send:   FF FF 01 02 06 F6
Return: FF FF 01 02 00 FC
```

---

### 4.7 SYNC WRITE — Synchronous Write

Write to the same register range on multiple servos in a single packet. All target servos must share the same start address and data length.

**Packet format**

```
FF FF  FE  <Length>  83  <StartAddr>  <DataLen(L)>
  <ID1>  <D1_1> … <D1_L>
  <ID2>  <D2_1> … <D2_L>
  …
  <CheckSum>
```

Where `Length = (L + 1) × N + 4` (N = number of servos)

**Example** — move servos ID 1–4 to position 2048, speed 1000 steps/s, time 0

```
FF FF FE 20 83 2A 06
  01 00 08 00 00 E8 03
  02 00 08 00 00 E8 03
  03 00 08 00 00 E8 03
  04 00 08 00 00 E8 03
58
```

Uses broadcast ID — no response is returned.

---

### 4.8 SYNC READ — Synchronous Read

Query the same register range from multiple servos in a single packet.

**Packet format**

```
FF FF  FE  <N+4>  82  <StartAddr>  <DataLen>
  <ID1>  <ID2>  …  <IDN>
  <CheckSum>
```

Each queried servo responds in ID order, one response packet per servo.

**Example** — query position + speed + load + voltage of servos ID 1 and ID 2 (address 0x38, 8 bytes)

```
Send:       FF FF FE 06 82 38 08 01 02 36

Return ID1: FF FF 01 0A 00  00 08  00 00  00 00  79  1E  55
Return ID2: FF FF 02 0A 00  FF 07  00 00  00 00  77  23  53
```

---

## 5. Signed Data Representation

For signed values (e.g. speed, position correction), negative numbers use **sign-magnitude** encoding: the MSB is the sign bit, the remaining bits are the magnitude.

**Example** — 16-bit value with BIT15 as the sign bit

| Value | Binary (BIT15…BIT0) |
|-------|---------------------|
| +2000 | `0 000011111010000` |
| −2000 | `1 000011111010000` |

---

## 6. Checksum Calculation

```python
def calc_checksum(packet: list[int]) -> int:
    # packet = [ID, Length, Instruction, P1, ..., PN]
    return (~sum(packet)) & 0xFF
```

---

## 7. Timing Recommendations

- After sending an instruction, wait at least **1 ms** before reading the response.
- Broadcast packets (ID = 0xFE) produce no response — no wait is needed.
- For multi-servo synchronous control, prefer **SYNC WRITE** over REG WRITE + ACTION; SYNC WRITE has better real-time performance.
