"""
hiwonder_servo_rpi.py — Raspberry Pi driver for HX-30HM bus servo

Uses /dev/ttyS0 (GPIO 14/15) or /dev/ttyUSB0.
On RPi 4/5 the default UART is /dev/ttyAMA0 — disable Bluetooth first:
  Add "dtoverlay=disable-bt" to /boot/config.txt and reboot.
"""

from __future__ import annotations
import time
from typing import Optional

# Re-use the generic Python SDK — it works on RPi unchanged.
import sys
import os
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', 'python'))

from hiwonder_servo import HiwonderBus, Servo, Reg, sync_move, BROADCAST_ID


# ---------------------------------------------------------------------------
# Raspberry Pi convenience wrapper (adds GPIO-based direction control if needed)
# ---------------------------------------------------------------------------

try:
    import RPi.GPIO as GPIO
    _HAS_GPIO = True
except ImportError:
    _HAS_GPIO = False


class RpiServoBus(HiwonderBus):
    """
    HiwonderBus variant that optionally toggles a GPIO pin for TX/RX direction
    control when using an RS-485/half-duplex driver chip.

    For most wiring (74HC126 or 1 kΩ series resistor), leave dir_pin=None.
    """

    def __init__(self, port: str = "/dev/ttyAMA0",
                 baudrate: int = 1_000_000,
                 dir_pin: Optional[int] = None,
                 timeout: float = 0.05):
        super().__init__(port, baudrate=baudrate, timeout=timeout)
        self._dir_pin = dir_pin
        if dir_pin is not None and _HAS_GPIO:
            GPIO.setmode(GPIO.BCM)
            GPIO.setup(dir_pin, GPIO.OUT, initial=GPIO.LOW)

    def _send(self, packet: bytes):
        if self._dir_pin is not None and _HAS_GPIO:
            GPIO.output(self._dir_pin, GPIO.HIGH)       # TX mode
            self._ser.reset_input_buffer()
            self._ser.write(packet)
            self._ser.flush()
            # wait for all bytes to clock out at 1 Mbps
            time.sleep(len(packet) * 10 / 1_000_000 + 0.001)
            GPIO.output(self._dir_pin, GPIO.LOW)        # RX mode
        else:
            super()._send(packet)

    def close(self):
        super().close()
        if self._dir_pin is not None and _HAS_GPIO:
            GPIO.cleanup(self._dir_pin)


__all__ = ["RpiServoBus", "Servo", "Reg", "sync_move", "BROADCAST_ID"]
