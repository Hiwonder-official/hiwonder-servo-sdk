"""
sync_control_rpi.py — Synchronously move multiple servos on Raspberry Pi.

Usage:
    python sync_control_rpi.py --port /dev/ttyAMA0 --ids 1 2 3 4
"""

import argparse
import time
import sys
import os

sys.path.insert(0, os.path.join(os.path.dirname(__file__), "../.."))
from raspberrypi.hiwonder_servo_rpi import RpiServoBus, Servo, sync_move


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--port", default="/dev/ttyAMA0")
    parser.add_argument("--ids", type=int, nargs="+", default=[1, 2, 3, 4])
    parser.add_argument("--dir-pin", type=int, default=None)
    args = parser.parse_args()

    bus = RpiServoBus(args.port, dir_pin=args.dir_pin)

    for sid in args.ids:
        print(f"Servo {sid}: {'online' if bus.ping(sid) else 'NOT FOUND'}")

    # Center all
    print("Centering...")
    sync_move(bus, [(sid, 2048, 0, 500) for sid in args.ids])
    time.sleep(1.0)

    # Wave
    for i in range(6):
        cmds = [(sid, (500 + i * 600 + j * 200) % 4096, 0, 400)
                for j, sid in enumerate(args.ids)]
        print(f"Wave {i+1}: {[c[1] for c in cmds]}")
        sync_move(bus, cmds)
        time.sleep(0.8)

    # Read all positions
    result = bus.sync_read(0x38, 2, args.ids)
    for sid, data in result.items():
        pos = data[0] | (data[1] << 8)
        print(f"Servo {sid} final position: {pos}")

    bus.close()


if __name__ == "__main__":
    main()
