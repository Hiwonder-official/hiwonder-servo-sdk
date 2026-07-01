"""
basic_motion.py — Move a single servo to several positions and print status.

Usage:
    python basic_motion.py --port COM3 --id 1
"""

import argparse
import time
import sys
import os

sys.path.insert(0, os.path.join(os.path.dirname(__file__), ".."))
from hiwonder_servo import HiwonderBus, Servo


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--port", default="COM3")
    parser.add_argument("--id", type=int, default=1)
    args = parser.parse_args()

    bus = HiwonderBus(args.port)
    servo = Servo(bus, args.id)

    print(f"Pinging servo {args.id}:", bus.ping(args.id))
    st = servo.full_status()
    print(f"Initial status: {st}")

    positions = [0, 1024, 2048, 3072, 4095]
    for pos in positions:
        print(f"Moving to {pos}...")
        servo.move_to(pos, speed=500)
        time.sleep(1.5)
        print(f"  current position={servo.position}, temp={servo.temperature}°C")

    print("Done.")
    bus.close()


if __name__ == "__main__":
    main()
