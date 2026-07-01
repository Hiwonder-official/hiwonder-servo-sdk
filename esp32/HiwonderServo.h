/*
 * HiwonderServo.h — ESP32 Arduino driver for HX-30HM bus servo
 *
 * Uses a single UART TX/RX pin in half-duplex mode via a direction-control
 * pin, or a 74HC126 buffer driven by the TX pin directly (recommend).
 *
 * Default wiring (UART2):
 *   GPIO 16 → RX
 *   GPIO 17 → TX  (also drives the bus through 74HC126 / 1k resistor)
 */

#pragma once
#include <Arduino.h>
#include <HardwareSerial.h>

// ---------------------------------------------------------------------------
// Register addresses
// ---------------------------------------------------------------------------
namespace Reg {
    constexpr uint8_t FIRMWARE_MAIN   = 0x00;
    constexpr uint8_t FIRMWARE_SUB    = 0x01;
    constexpr uint8_t MODEL_L         = 0x03;
    constexpr uint8_t MODEL_H         = 0x04;
    constexpr uint8_t ID              = 0x05;
    constexpr uint8_t BAUD_RATE       = 0x06;
    constexpr uint8_t RESPONSE_LEVEL  = 0x08;
    constexpr uint8_t TEMP_LIMIT      = 0x0D;
    constexpr uint8_t OVERVOLT_LIMIT  = 0x0E;   // 2 bytes
    constexpr uint8_t UNDERVOLT_LIMIT = 0x10;   // 2 bytes
    constexpr uint8_t MAX_TORQUE      = 0x13;   // 2 bytes
    constexpr uint8_t PROTECT_CTRL    = 0x14;
    constexpr uint8_t LED_ALARM       = 0x15;
    constexpr uint8_t POS_P           = 0x16;   // 2 bytes
    constexpr uint8_t POS_D           = 0x17;   // 2 bytes
    constexpr uint8_t POS_I           = 0x18;   // 2 bytes
    constexpr uint8_t MIN_STARTUP     = 0x1A;   // 2 bytes
    constexpr uint8_t CW_DEADZONE     = 0x1B;
    constexpr uint8_t CCW_DEADZONE    = 0x1C;
    constexpr uint8_t CURRENT_LIMIT   = 0x1F;   // 2 bytes, mA
    constexpr uint8_t CONTROL_MODE    = 0x21;   // 2 bytes
    constexpr uint8_t OVERLOAD_TORQUE = 0x22;
    constexpr uint8_t OVERLOAD_TIME   = 0x23;
    constexpr uint8_t OVERLOAD_THR    = 0x24;   // 2 bytes
    constexpr uint8_t SPD_P           = 0x25;   // 2 bytes
    constexpr uint8_t OVERCURRENT_TIME= 0x26;   // 2 bytes
    constexpr uint8_t SPD_I           = 0x27;   // 2 bytes
    constexpr uint8_t TARGET_POS      = 0x2A;   // 2 bytes  ← position cmd
    constexpr uint8_t MOVE_TIME       = 0x2C;   // 2 bytes, ms
    constexpr uint8_t MOVE_SPEED      = 0x2E;   // 2 bytes, steps/s
    constexpr uint8_t NVS_LOCK        = 0x37;
    constexpr uint8_t CURRENT_POS     = 0x38;   // 2 bytes, RO
    constexpr uint8_t CURRENT_SPD     = 0x3A;   // 2 bytes, RO
    constexpr uint8_t CURRENT_LOAD    = 0x3C;   // 2 bytes, RO
    constexpr uint8_t CURRENT_VOLT    = 0x3E;   // 2 bytes, RO
    constexpr uint8_t CURRENT_TEMP    = 0x3F;   // 1 byte,  RO
    constexpr uint8_t REG_FLAG        = 0x40;
    constexpr uint8_t STATUS          = 0x41;
    constexpr uint8_t MOVING          = 0x42;
    constexpr uint8_t CURRENT_AMP     = 0x45;   // 2 bytes, RO
}

constexpr uint8_t BROADCAST_ID = 0xFE;

// ---------------------------------------------------------------------------
// Status structure returned by fullStatus()
// ---------------------------------------------------------------------------
struct ServoStatus {
    uint16_t position;   // steps
    uint16_t speed;      // steps/s
    uint16_t load;       // ‰ rated torque
    float    voltage;    // V
    uint8_t  temperature; // °C
};

// ---------------------------------------------------------------------------
// HiwonderBus — low-level UART bus driver
// ---------------------------------------------------------------------------
class HiwonderBus {
public:
    explicit HiwonderBus(HardwareSerial& serial,
                         int rxPin = 16, int txPin = 17,
                         uint32_t baud = 1000000UL)
        : _serial(serial)
    {
        _serial.begin(baud, SERIAL_8N1, rxPin, txPin);
    }

    // ----- Core instructions -----------------------------------------------

    bool ping(uint8_t id);

    bool read(uint8_t id, uint8_t startAddr, uint8_t len, uint8_t* outBuf);

    bool write(uint8_t id, uint8_t startAddr, const uint8_t* data, uint8_t len);

    bool regWrite(uint8_t id, uint8_t startAddr, const uint8_t* data, uint8_t len);

    void action();

    void reset(uint8_t id);

    /*
     * syncWrite — write same register block to multiple servos.
     *
     * ids[]:     servo ID array
     * dataArr[]: concatenated payload for each servo (dataLen bytes each)
     * count:     number of servos
     * dataLen:   bytes per servo (e.g. 6 for pos+time+speed)
     */
    bool syncWrite(uint8_t startAddr, uint8_t dataLen,
                   const uint8_t* ids, const uint8_t* dataArr, uint8_t count);

    // ----- Convenience wrappers --------------------------------------------

    uint16_t readWord(uint8_t id, uint8_t addr);
    bool     writeWord(uint8_t id, uint8_t addr, uint16_t value);

private:
    HardwareSerial& _serial;
    uint8_t _buf[256];

    static uint8_t checksum(const uint8_t* payload, uint8_t len);
    uint8_t buildPacket(uint8_t* out, uint8_t id, uint8_t instr,
                        const uint8_t* params, uint8_t paramLen);
    bool    recvPacket(uint8_t expectedParams, uint8_t& errorOut, uint8_t* paramBuf);
};

// ---------------------------------------------------------------------------
// Servo — high-level single-servo API
// ---------------------------------------------------------------------------
class Servo {
public:
    Servo(HiwonderBus& bus, uint8_t id) : _bus(bus), _id(id) {}

    // Motion
    void moveTo(uint16_t position, uint16_t speed = 0, uint16_t timeMs = 0);
    void stop();

    // Status
    uint16_t    position();
    uint16_t    speed();
    uint16_t    load();
    float       voltage();
    uint8_t     temperature();
    uint16_t    current();
    bool        isMoving();
    ServoStatus fullStatus();

    // Configuration (NVS)
    void setId(uint8_t newId);
    void setBaudRate(uint8_t index);
    void setMaxTorque(uint16_t permille);
    void torqueEnable(bool enable);
    void setPid(uint16_t p, uint16_t i, uint16_t d);

private:
    HiwonderBus& _bus;
    uint8_t      _id;
    void _nvsWrite(uint8_t addr, const uint8_t* data, uint8_t len);
};

// ---------------------------------------------------------------------------
// syncMove — helper to move multiple servos simultaneously
// ---------------------------------------------------------------------------
void syncMove(HiwonderBus& bus, const uint8_t* ids,
              const uint16_t* positions, const uint16_t* speeds,
              const uint16_t* timesMs, uint8_t count);
