/*
 * sync_control.ino — Synchronously control 4 servos (IDs 1–4).
 */

#include "../HiwonderServo.h"

HiwonderBus bus(Serial2, 16, 17, 1000000UL);

constexpr uint8_t  NUM_SERVOS = 4;
const     uint8_t  IDS[NUM_SERVOS] = {1, 2, 3, 4};

void setup() {
    Serial.begin(115200);
    delay(500);

    for (uint8_t i = 0; i < NUM_SERVOS; i++) {
        bool ok = bus.ping(IDS[i]);
        Serial.printf("Servo %d: %s\n", IDS[i], ok ? "online" : "NOT FOUND");
    }

    // Move all to center
    uint16_t pos[NUM_SERVOS] = {2048, 2048, 2048, 2048};
    uint16_t spd[NUM_SERVOS] = {500,  500,  500,  500};
    uint16_t tim[NUM_SERVOS] = {0,    0,    0,    0};
    syncMove(bus, IDS, pos, spd, tim, NUM_SERVOS);
    delay(1000);
}

void loop() {
    // Wave pattern
    static uint8_t step = 0;
    const uint16_t wave[4][4] = {
        {500,  1500, 2500, 3500},
        {1000, 2000, 3000, 4000},
        {2048, 2048, 2048, 2048},
        {3500, 2500, 1500,  500},
    };

    uint16_t pos[NUM_SERVOS];
    uint16_t spd[NUM_SERVOS] = {400, 400, 400, 400};
    uint16_t tim[NUM_SERVOS] = {0, 0, 0, 0};
    for (uint8_t i = 0; i < NUM_SERVOS; i++) pos[i] = wave[step][i];

    Serial.printf("Step %d: %d %d %d %d\n", step, pos[0], pos[1], pos[2], pos[3]);
    syncMove(bus, IDS, pos, spd, tim, NUM_SERVOS);
    step = (step + 1) % 4;
    delay(1000);
}
