"""
sync_control.py — Synchronously control multiple servos.

Usage:
    python sync_control.py --port COM3 --ids 1 2 3 4
"""

import argparse
import time
import sys
import os

sys.path.insert(0, os.path.join(os.path.dirname(__file__), ".."))
from hiwonder_servo import HiwonderBus, Servo, sync_move


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--port", default="COM3")
    parser.add_argument("--ids", type=int, nargs="+", default=[1, 2, 3, 4])
    args = parser.parse_args()

    bus = HiwonderBus(args.port)

    # Verify all servos are online
    for sid in args.ids:
        ok = bus.ping(sid)
        print(f"Servo {sid}: {'online' if ok else 'NOT FOUND'}")

    # Move all to center
    print("\nMoving all to center (2048)...")
    commands = [(sid, 2048, 500, 0) for sid in args.ids]
    sync_move(bus, commands)
    time.sleep(1.0)

    # Move in a wave pattern
    wave_positions = [500, 1000, 2048, 3000, 3500]
    for i, pos in enumerate(wave_positions):
        cmds = [(sid, (pos + i * 200) % 4096, 300, 0) for i, sid in enumerate(args.ids)]
        print(f"Wave step {i+1}: {[c[1] for c in cmds]}")
        sync_move(bus, cmds)
        time.sleep(0.8)

    # Sync read current positions
    print("\nSync reading positions...")
    result = bus.sync_read(0x38, 2, args.ids)
    for sid, data in result.items():
        pos = data[0] | (data[1] << 8)
        print(f"  Servo {sid}: position={pos}")

    bus.close()


if __name__ == "__main__":
    main()
