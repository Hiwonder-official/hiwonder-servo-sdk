"""
basic_motion_rpi.py — Move a servo on Raspberry Pi.

Usage:
    python basic_motion_rpi.py --port /dev/ttyAMA0 --id 1
"""

import argparse
import time
import sys
import os

sys.path.insert(0, os.path.join(os.path.dirname(__file__), ".."))
from raspberrypi.hiwonder_servo_rpi import RpiServoBus, Servo


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--port", default="/dev/ttyAMA0")
    parser.add_argument("--id", type=int, default=1)
    parser.add_argument("--dir-pin", type=int, default=None,
                        help="BCM GPIO pin for TX/RX direction (optional)")
    args = parser.parse_args()

    bus = RpiServoBus(args.port, dir_pin=args.dir_pin)
    servo = Servo(bus, args.id)

    print(f"Ping ID {args.id}:", bus.ping(args.id))
    print("Status:", servo.full_status())

    for pos in [0, 1024, 2048, 3072, 4095]:
        print(f"Moving to {pos}...")
        servo.move_to(pos, speed=400)
        time.sleep(1.5)
        print(f"  pos={servo.position()}, temp={servo.temperature()}°C")

    bus.close()


if __name__ == "__main__":
    main()
