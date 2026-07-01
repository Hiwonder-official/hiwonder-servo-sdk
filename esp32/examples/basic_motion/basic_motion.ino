/*
 * basic_motion.ino — Move a single HX-30HM servo through several positions.
 *
 * Wiring:
 *   Servo bus data line → GPIO 16 (RX) and GPIO 17 (TX) through a 74HC126
 *   or a 1 kΩ resistor from TX → bus, RX directly on bus.
 */

#include "../HiwonderServo.h"

HiwonderBus bus(Serial2, 16, 17, 1000000UL);
Servo       servo(bus, 1);   // change to your servo ID

void setup() {
    Serial.begin(115200);
    delay(500);

    Serial.println("Pinging servo 1...");
    bool found = bus.ping(1);
    Serial.printf("Ping: %s\n", found ? "OK" : "no response");

    ServoStatus st = servo.fullStatus();
    Serial.printf("pos=%d  volt=%.1fV  temp=%d°C\n",
                  st.position, st.voltage, st.temperature);
}

void loop() {
    static const uint16_t positions[] = {0, 1024, 2048, 3072, 4095};
    static uint8_t idx = 0;

    uint16_t pos = positions[idx];
    Serial.printf("Moving to %d\n", pos);
    servo.moveTo(pos, 500, 0);  // speed=500 steps/s
    delay(1500);

    Serial.printf("  current position = %d\n", servo.position());
    idx = (idx + 1) % 5;
}
