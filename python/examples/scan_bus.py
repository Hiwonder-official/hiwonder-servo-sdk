"""
scan_bus.py — Scan all IDs on the bus and print servo info.

Usage:
    python scan_bus.py --port COM3
"""

import argparse
import sys
import os

sys.path.insert(0, os.path.join(os.path.dirname(__file__), ".."))
from hiwonder_servo import HiwonderBus, Servo


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--port", default="COM3")
    parser.add_argument("--start", type=int, default=0)
    parser.add_argument("--end", type=int, default=253)
    args = parser.parse_args()

    bus = HiwonderBus(args.port)
    found = []

    print(f"Scanning IDs {args.start}–{args.end} on {args.port}...")
    for sid in range(args.start, args.end + 1):
        if bus.ping(sid):
            found.append(sid)
            servo = Servo(bus, sid)
            try:
                st = servo.full_status()
                print(f"  ID {sid:3d}: pos={st['position']:4d}  volt={st['voltage']:.1f}V  temp={st['temperature']}°C")
            except Exception as e:
                print(f"  ID {sid:3d}: found (read error: {e})")

    print(f"\nFound {len(found)} servo(s): {found}")
    bus.close()


if __name__ == "__main__":
    main()
