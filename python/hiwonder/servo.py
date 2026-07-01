"""
hiwonder/servo.py — Python SDK for HX-30HM bus servo
Protocol: UART half-duplex, 1 Mbps, little-endian
"""

from __future__ import annotations
import serial
import time
from typing import Optional


class Reg:
    FIRMWARE_MAIN   = 0x00
    FIRMWARE_SUB    = 0x01
    MODEL_L         = 0x03
    MODEL_H         = 0x04
    ID              = 0x05
    BAUD_RATE       = 0x06
    RESPONSE_LEVEL  = 0x08
    TEMP_LIMIT      = 0x0D
    OVERVOLT_LIMIT  = 0x0E   # 2 bytes, unit 0.1 V
    UNDERVOLT_LIMIT = 0x10   # 2 bytes, unit 0.1 V
    MAX_TORQUE      = 0x13   # 2 bytes, ‰ (1000 = 100%)
    PROTECT_CTRL    = 0x14
    LED_ALARM       = 0x15
    POS_P           = 0x16   # 2 bytes
    POS_D           = 0x17   # 2 bytes
    POS_I           = 0x18   # 2 bytes
    MIN_STARTUP     = 0x1A   # 2 bytes
    CW_DEADZONE     = 0x1B
    CCW_DEADZONE    = 0x1C
    CURRENT_LIMIT   = 0x1F   # 2 bytes, mA
    CONTROL_MODE    = 0x21   # 2 bytes
    OVERLOAD_TORQUE = 0x22
    OVERLOAD_TIME   = 0x23
    OVERLOAD_THR    = 0x24   # 2 bytes
    SPD_P           = 0x25   # 2 bytes
    OVERCURRENT_TIME = 0x26  # 2 bytes, ms
    SPD_I           = 0x27   # 2 bytes
    # 0x28/0x29 — simple target pos + time (independent, non-atomic writes)
    SIMPLE_POS      = 0x28   # 2 bytes — target position, non-atomic variant
    SIMPLE_TIME     = 0x29   # 2 bytes — run time paired with 0x28 only
    # 0x2A/0x2C/0x2E — RECOMMENDED: atomic 6-byte block, write all three at once
    TARGET_POS      = 0x2A   # 2 bytes — target position (atomic block start)
    MOVE_TIME       = 0x2C   # 2 bytes, ms (also PWM open-loop duty)
    MOVE_SPEED      = 0x2E   # 2 bytes, steps/s
    NVS_LOCK        = 0x37   # 0=unlock, 1=lock (default)
    CURRENT_POS     = 0x38   # 2 bytes, read-only
    CURRENT_SPD     = 0x3A   # 2 bytes, read-only
    CURRENT_LOAD    = 0x3C   # 2 bytes, read-only
    CURRENT_VOLT    = 0x3E   # 2 bytes, read-only, 0.1 V
    CURRENT_TEMP    = 0x3F   # 1 byte, read-only, °C
    REG_FLAG        = 0x40   # async write flag
    STATUS          = 0x41   # servo status bits
    MOVING          = 0x42   # 1 = in motion
    CURRENT_AMP     = 0x45   # 2 bytes, mA


BROADCAST_ID = 0xFE


class ServoError(Exception):
    pass


class HiwonderBus:
    """
    Low-level bus driver. Handles packet framing, checksum, and half-duplex
    direction switching on a single UART port.
    """

    def __init__(self, port: str, baudrate: int = 1_000_000, timeout: float = 0.05):
        self._ser = serial.Serial(
            port,
            baudrate=baudrate,
            bytesize=serial.EIGHTBITS,
            parity=serial.PARITY_NONE,
            stopbits=serial.STOPBITS_ONE,
            timeout=timeout,
        )

    def close(self):
        self._ser.close()

    # ------------------------------------------------------------------
    # Packet helpers
    # ------------------------------------------------------------------
    @staticmethod
    def _checksum(payload: list[int]) -> int:
        return (~sum(payload)) & 0xFF

    def _build_packet(self, servo_id: int, instruction: int, params: list[int]) -> bytes:
        length = len(params) + 2
        payload = [servo_id, length, instruction] + params
        cs = self._checksum(payload)
        return bytes([0xFF, 0xFF] + payload + [cs])

    def _send(self, packet: bytes):
        self._ser.reset_input_buffer()
        self._ser.write(packet)

    def _recv(self, expected_params: int) -> tuple[int, int, list[int]]:
        """Read one response packet. Returns (servo_id, error, params)."""
        buf = b""
        deadline = time.monotonic() + self._ser.timeout
        while time.monotonic() < deadline:
            b = self._ser.read(1)
            if not b:
                break
            buf += b
            if len(buf) >= 2 and buf[-2:] == b"\xff\xff":
                break

        rest = self._ser.read(4 + expected_params)
        if len(rest) < 4:
            raise ServoError("Timeout: no response")

        servo_id = rest[0]
        length   = rest[1]
        error    = rest[2]
        params   = list(rest[3 : 3 + length - 2])
        checksum = rest[3 + length - 2] if len(rest) > 3 + length - 2 else 0

        payload = [servo_id, length, error] + params
        if self._checksum(payload) != checksum:
            raise ServoError(f"Checksum mismatch (id={servo_id})")

        if error:
            raise ServoError(f"Servo {servo_id} error flags: 0x{error:02X}")

        return servo_id, error, params

    # ------------------------------------------------------------------
    # Core instructions
    # ------------------------------------------------------------------
    def ping(self, servo_id: int) -> bool:
        """Return True if the servo responds."""
        pkt = self._build_packet(servo_id, 0x01, [])
        self._send(pkt)
        try:
            self._recv(0)
            return True
        except ServoError:
            return False

    def read(self, servo_id: int, start_addr: int, length: int) -> list[int]:
        """Read *length* bytes starting at *start_addr*."""
        pkt = self._build_packet(servo_id, 0x02, [start_addr, length])
        self._send(pkt)
        _, _, params = self._recv(length)
        return params

    def write(self, servo_id: int, start_addr: int, data: list[int]):
        """Write bytes to the servo's memory table."""
        pkt = self._build_packet(servo_id, 0x03, [start_addr] + data)
        self._send(pkt)
        if servo_id != BROADCAST_ID:
            self._recv(0)

    def reg_write(self, servo_id: int, start_addr: int, data: list[int]):
        """Async write — buffers data until ACTION is called."""
        pkt = self._build_packet(servo_id, 0x04, [start_addr] + data)
        self._send(pkt)
        if servo_id != BROADCAST_ID:
            self._recv(0)

    def action(self):
        """Broadcast ACTION — all servos execute their buffered REG WRITE."""
        pkt = self._build_packet(BROADCAST_ID, 0x05, [])
        self._send(pkt)

    def reset(self, servo_id: int):
        """Reset servo to factory defaults."""
        pkt = self._build_packet(servo_id, 0x06, [])
        self._send(pkt)
        if servo_id != BROADCAST_ID:
            self._recv(0)

    def sync_write(self, start_addr: int, data_len: int, servo_data: list[tuple[int, list[int]]]):
        """
        Synchronously write the same register range across multiple servos.

        servo_data: list of (servo_id, [d1, d2, ...]) where len == data_len
        """
        params = [start_addr, data_len]
        for sid, values in servo_data:
            if len(values) != data_len:
                raise ValueError(f"Servo {sid}: expected {data_len} bytes, got {len(values)}")
            params += [sid] + values
        pkt = self._build_packet(BROADCAST_ID, 0x83, params)
        self._send(pkt)

    def sync_read(self, start_addr: int, data_len: int, servo_ids: list[int]) -> dict[int, list[int]]:
        """
        Synchronously read the same register range from multiple servos.
        Returns {servo_id: [bytes...]} in ID order.
        """
        params = [start_addr, data_len] + servo_ids
        pkt = self._build_packet(BROADCAST_ID, 0x82, params)
        self._send(pkt)
        result = {}
        for sid in servo_ids:
            _, _, values = self._recv(data_len)
            result[sid] = values
        return result

    # ------------------------------------------------------------------
    # Helper: read/write 16-bit little-endian value
    # ------------------------------------------------------------------
    def read_word(self, servo_id: int, addr: int) -> int:
        d = self.read(servo_id, addr, 2)
        return d[0] | (d[1] << 8)

    def write_word(self, servo_id: int, addr: int, value: int):
        self.write(servo_id, addr, [value & 0xFF, (value >> 8) & 0xFF])


class Servo:
    """
    High-level API for a single HX-30HM servo.

    Example::

        bus = HiwonderBus("COM3")
        s = Servo(bus, servo_id=1)
        s.move_to(2048, speed=500)
        print(s.position)
    """

    def __init__(self, bus: HiwonderBus, servo_id: int):
        self.bus = bus
        self.id  = servo_id

    # ------------------------------------------------------------------
    # Motion control
    # ------------------------------------------------------------------
    def move_to(self, position: int, speed: int = 0, time_ms: int = 0):
        """
        Move to absolute *position* (0–4095).

        Specify either *speed* (steps/s) or *time_ms* (ms), not both.
        If both are 0 the servo moves at maximum speed.

        Uses the atomic 6-byte block at 0x2A (TARGET_POS + MOVE_TIME + MOVE_SPEED)
        so position, time, and speed all take effect simultaneously.
        """
        pos_l, pos_h = position & 0xFF, (position >> 8) & 0xFF
        t_l,   t_h   = time_ms & 0xFF, (time_ms >> 8) & 0xFF
        s_l,   s_h   = speed & 0xFF,   (speed >> 8) & 0xFF
        self.bus.write(self.id, Reg.TARGET_POS, [pos_l, pos_h, t_l, t_h, s_l, s_h])

    def stop(self):
        """Immediately stop by writing current position as target."""
        pos = self.position
        self.move_to(pos)

    # ------------------------------------------------------------------
    # Status reads
    # ------------------------------------------------------------------
    @property
    def position(self) -> int:
        """Current position in steps (0–4095)."""
        return self.bus.read_word(self.id, Reg.CURRENT_POS)

    @property
    def speed(self) -> int:
        """Current speed in steps/s."""
        return self.bus.read_word(self.id, Reg.CURRENT_SPD)

    @property
    def load(self) -> int:
        """Current load in ‰ of rated torque."""
        return self.bus.read_word(self.id, Reg.CURRENT_LOAD)

    @property
    def voltage(self) -> float:
        """Supply voltage in volts."""
        return self.bus.read_word(self.id, Reg.CURRENT_VOLT) * 0.1

    @property
    def temperature(self) -> int:
        """Internal temperature in °C."""
        return self.bus.read(self.id, Reg.CURRENT_TEMP, 1)[0]

    @property
    def current(self) -> int:
        """Motor current in mA."""
        return self.bus.read_word(self.id, Reg.CURRENT_AMP)

    @property
    def is_moving(self) -> bool:
        return bool(self.bus.read(self.id, Reg.MOVING, 1)[0])

    def full_status(self) -> dict:
        """Read position, speed, load, voltage, temperature in one packet."""
        data = self.bus.read(self.id, Reg.CURRENT_POS, 8)
        pos  = data[0] | (data[1] << 8)
        spd  = data[2] | (data[3] << 8)
        load = data[4] | (data[5] << 8)
        volt = (data[6] | (data[7] << 8)) * 0.1
        temp = self.bus.read(self.id, Reg.CURRENT_TEMP, 1)[0]
        return {"position": pos, "speed": spd, "load": load, "voltage": volt, "temperature": temp}

    # ------------------------------------------------------------------
    # Configuration
    # ------------------------------------------------------------------
    def set_id(self, new_id: int):
        """Change the servo's ID (persists after power cycle)."""
        self._nvs_write(Reg.ID, [new_id])

    def set_baud_rate(self, index: int):
        """
        Set baud rate index (0–7).
        0=1Mbps, 1=500k, 2=250k, 3=128k, 4=115200, 5=76800, 6=57600, 7=38400
        """
        self._nvs_write(Reg.BAUD_RATE, [index])

    def set_max_torque(self, permille: int):
        """Max torque as ‰ of rated (0–1000, 1000 = 100%)."""
        self._nvs_write(Reg.MAX_TORQUE, [permille & 0xFF, (permille >> 8) & 0xFF])

    def torque_enable(self, enabled: bool):
        """Enable or disable torque output (via max torque register)."""
        if enabled:
            self.set_max_torque(1000)
        else:
            self.set_max_torque(0)

    def set_pid(self, p: int, i: int, d: int):
        """Set position PID coefficients (stored in NVS)."""
        self._nvs_write(Reg.POS_P, [p & 0xFF, (p >> 8) & 0xFF])
        self._nvs_write(Reg.POS_D, [d & 0xFF, (d >> 8) & 0xFF])
        self._nvs_write(Reg.POS_I, [i & 0xFF, (i >> 8) & 0xFF])

    def _nvs_write(self, addr: int, data: list[int]):
        self.bus.write(self.id, Reg.NVS_LOCK, [0])     # unlock
        self.bus.write(self.id, addr, data)
        self.bus.write(self.id, Reg.NVS_LOCK, [1])     # re-lock


def sync_move(bus: HiwonderBus, commands: list[tuple[int, int, int, int]]):
    """
    Move multiple servos simultaneously using SYNC WRITE.

    commands: list of (servo_id, position, time_ms, speed)

    Writes the atomic 6-byte block (0x2A–0x2F) for each servo in one SYNC WRITE
    packet so all servos start moving at the same instant.
    """
    servo_data = []
    for sid, pos, t_ms, spd in commands:
        payload = [
            pos & 0xFF, (pos >> 8) & 0xFF,
            t_ms & 0xFF, (t_ms >> 8) & 0xFF,
            spd & 0xFF, (spd >> 8) & 0xFF,
        ]
        servo_data.append((sid, payload))
    bus.sync_write(Reg.TARGET_POS, 6, servo_data)
