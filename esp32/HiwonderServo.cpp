/*
 * HiwonderServo.cpp — ESP32 implementation
 */

#include "HiwonderServo.h"

// ---------------------------------------------------------------------------
// HiwonderBus — private helpers
// ---------------------------------------------------------------------------

uint8_t HiwonderBus::checksum(const uint8_t* payload, uint8_t len) {
    uint16_t sum = 0;
    for (uint8_t i = 0; i < len; i++) sum += payload[i];
    return (~sum) & 0xFF;
}

uint8_t HiwonderBus::buildPacket(uint8_t* out, uint8_t id, uint8_t instr,
                                  const uint8_t* params, uint8_t paramLen) {
    uint8_t length = paramLen + 2;
    out[0] = 0xFF;
    out[1] = 0xFF;
    out[2] = id;
    out[3] = length;
    out[4] = instr;
    for (uint8_t i = 0; i < paramLen; i++) out[5 + i] = params[i];
    // checksum covers id, length, instr, params
    uint8_t cs_buf[64];
    cs_buf[0] = id;
    cs_buf[1] = length;
    cs_buf[2] = instr;
    memcpy(cs_buf + 3, params, paramLen);
    out[5 + paramLen] = checksum(cs_buf, 3 + paramLen);
    return 6 + paramLen;
}

bool HiwonderBus::recvPacket(uint8_t expectedParams, uint8_t& errorOut, uint8_t* paramBuf) {
    uint32_t deadline = millis() + 50;

    // Sync to 0xFF 0xFF header
    uint8_t prev = 0;
    while (millis() < deadline) {
        if (!_serial.available()) continue;
        uint8_t b = _serial.read();
        if (prev == 0xFF && b == 0xFF) break;
        prev = b;
    }
    if (millis() >= deadline) return false;

    // id, length, error, params…, checksum
    uint8_t minBytes = 4 + expectedParams;
    uint32_t t = millis() + 30;
    uint8_t buf[64];
    uint8_t n = 0;
    while (n < minBytes && millis() < t) {
        if (_serial.available()) buf[n++] = _serial.read();
    }
    if (n < minBytes) return false;

    uint8_t id     = buf[0];
    uint8_t length = buf[1];
    errorOut       = buf[2];

    uint8_t cs_buf[64];
    cs_buf[0] = id;
    cs_buf[1] = length;
    cs_buf[2] = errorOut;
    for (uint8_t i = 0; i < length - 2; i++) {
        cs_buf[3 + i] = buf[3 + i];
        if (paramBuf && i < expectedParams) paramBuf[i] = buf[3 + i];
    }
    uint8_t expected_cs = checksum(cs_buf, 2 + length);
    uint8_t recv_cs     = buf[2 + length];
    return expected_cs == recv_cs;
}

// ---------------------------------------------------------------------------
// HiwonderBus — public interface
// ---------------------------------------------------------------------------

bool HiwonderBus::ping(uint8_t id) {
    uint8_t pkt[8];
    uint8_t n = buildPacket(pkt, id, 0x01, nullptr, 0);
    _serial.flush();
    _serial.write(pkt, n);
    uint8_t err = 0;
    return recvPacket(0, err, nullptr);
}

bool HiwonderBus::read(uint8_t id, uint8_t startAddr, uint8_t len, uint8_t* outBuf) {
    uint8_t params[2] = {startAddr, len};
    uint8_t pkt[16];
    uint8_t n = buildPacket(pkt, id, 0x02, params, 2);
    _serial.flush();
    _serial.write(pkt, n);
    uint8_t err = 0;
    return recvPacket(len, err, outBuf);
}

bool HiwonderBus::write(uint8_t id, uint8_t startAddr, const uint8_t* data, uint8_t len) {
    uint8_t params[64];
    params[0] = startAddr;
    memcpy(params + 1, data, len);
    uint8_t pkt[80];
    uint8_t n = buildPacket(pkt, id, 0x03, params, len + 1);
    _serial.flush();
    _serial.write(pkt, n);
    if (id == BROADCAST_ID) return true;
    uint8_t err = 0;
    return recvPacket(0, err, nullptr);
}

bool HiwonderBus::regWrite(uint8_t id, uint8_t startAddr, const uint8_t* data, uint8_t len) {
    uint8_t params[64];
    params[0] = startAddr;
    memcpy(params + 1, data, len);
    uint8_t pkt[80];
    uint8_t n = buildPacket(pkt, id, 0x04, params, len + 1);
    _serial.flush();
    _serial.write(pkt, n);
    if (id == BROADCAST_ID) return true;
    uint8_t err = 0;
    return recvPacket(0, err, nullptr);
}

void HiwonderBus::action() {
    uint8_t pkt[8];
    uint8_t n = buildPacket(pkt, BROADCAST_ID, 0x05, nullptr, 0);
    _serial.write(pkt, n);
}

void HiwonderBus::reset(uint8_t id) {
    uint8_t pkt[8];
    uint8_t n = buildPacket(pkt, id, 0x06, nullptr, 0);
    _serial.flush();
    _serial.write(pkt, n);
    if (id == BROADCAST_ID) return;
    uint8_t err = 0;
    recvPacket(0, err, nullptr);
}

bool HiwonderBus::syncWrite(uint8_t startAddr, uint8_t dataLen,
                             const uint8_t* ids, const uint8_t* dataArr, uint8_t count) {
    uint8_t params[128];
    uint8_t pi = 0;
    params[pi++] = startAddr;
    params[pi++] = dataLen;
    for (uint8_t i = 0; i < count; i++) {
        params[pi++] = ids[i];
        memcpy(params + pi, dataArr + i * dataLen, dataLen);
        pi += dataLen;
    }
    uint8_t pkt[150];
    uint8_t n = buildPacket(pkt, BROADCAST_ID, 0x83, params, pi);
    _serial.write(pkt, n);
    return true;
}

uint16_t HiwonderBus::readWord(uint8_t id, uint8_t addr) {
    uint8_t buf[2] = {0, 0};
    read(id, addr, 2, buf);
    return (uint16_t)buf[0] | ((uint16_t)buf[1] << 8);
}

bool HiwonderBus::writeWord(uint8_t id, uint8_t addr, uint16_t value) {
    uint8_t data[2] = {(uint8_t)(value & 0xFF), (uint8_t)(value >> 8)};
    return write(id, addr, data, 2);
}

// ---------------------------------------------------------------------------
// Servo — implementation
// ---------------------------------------------------------------------------

void Servo::moveTo(uint16_t position, uint16_t speed, uint16_t timeMs) {
    uint8_t data[6] = {
        (uint8_t)(position & 0xFF), (uint8_t)(position >> 8),
        (uint8_t)(timeMs   & 0xFF), (uint8_t)(timeMs   >> 8),
        (uint8_t)(speed    & 0xFF), (uint8_t)(speed    >> 8),
    };
    _bus.write(_id, Reg::TARGET_POS, data, 6);
}

void Servo::stop() {
    uint16_t cur = position();
    moveTo(cur);
}

uint16_t Servo::position()    { return _bus.readWord(_id, Reg::CURRENT_POS); }
uint16_t Servo::speed()       { return _bus.readWord(_id, Reg::CURRENT_SPD); }
uint16_t Servo::load()        { return _bus.readWord(_id, Reg::CURRENT_LOAD); }
float    Servo::voltage()     { return _bus.readWord(_id, Reg::CURRENT_VOLT) * 0.1f; }
uint16_t Servo::current()     { return _bus.readWord(_id, Reg::CURRENT_AMP); }

uint8_t Servo::temperature() {
    uint8_t buf[1];
    _bus.read(_id, Reg::CURRENT_TEMP, 1, buf);
    return buf[0];
}

bool Servo::isMoving() {
    uint8_t buf[1];
    _bus.read(_id, Reg::MOVING, 1, buf);
    return buf[0] != 0;
}

ServoStatus Servo::fullStatus() {
    uint8_t buf[8];
    _bus.read(_id, Reg::CURRENT_POS, 8, buf);
    uint8_t tmp[1];
    _bus.read(_id, Reg::CURRENT_TEMP, 1, tmp);
    ServoStatus st;
    st.position    = (uint16_t)buf[0] | ((uint16_t)buf[1] << 8);
    st.speed       = (uint16_t)buf[2] | ((uint16_t)buf[3] << 8);
    st.load        = (uint16_t)buf[4] | ((uint16_t)buf[5] << 8);
    st.voltage     = ((uint16_t)buf[6] | ((uint16_t)buf[7] << 8)) * 0.1f;
    st.temperature = tmp[0];
    return st;
}

void Servo::setId(uint8_t newId)            { _nvsWrite(Reg::ID, &newId, 1); }
void Servo::setBaudRate(uint8_t index)      { _nvsWrite(Reg::BAUD_RATE, &index, 1); }
void Servo::torqueEnable(bool enable)       { setMaxTorque(enable ? 1000 : 0); }
void Servo::setMaxTorque(uint16_t permille) {
    uint8_t d[2] = {(uint8_t)(permille & 0xFF), (uint8_t)(permille >> 8)};
    _nvsWrite(Reg::MAX_TORQUE, d, 2);
}

void Servo::setPid(uint16_t p, uint16_t i, uint16_t d) {
    uint8_t pd[2], id[2], dd[2];
    pd[0] = p & 0xFF; pd[1] = p >> 8;
    id[0] = i & 0xFF; id[1] = i >> 8;
    dd[0] = d & 0xFF; dd[1] = d >> 8;
    _nvsWrite(Reg::POS_P, pd, 2);
    _nvsWrite(Reg::POS_D, dd, 2);
    _nvsWrite(Reg::POS_I, id, 2);
}

void Servo::_nvsWrite(uint8_t addr, const uint8_t* data, uint8_t len) {
    uint8_t unlock = 0, lock = 1;
    _bus.write(_id, Reg::NVS_LOCK, &unlock, 1);
    _bus.write(_id, addr, data, len);
    _bus.write(_id, Reg::NVS_LOCK, &lock, 1);
}

// ---------------------------------------------------------------------------
// syncMove helper
// ---------------------------------------------------------------------------

void syncMove(HiwonderBus& bus, const uint8_t* ids,
              const uint16_t* positions, const uint16_t* speeds,
              const uint16_t* timesMs, uint8_t count) {
    uint8_t dataArr[64 * 6];
    for (uint8_t i = 0; i < count; i++) {
        uint8_t* p = dataArr + i * 6;
        p[0] = positions[i] & 0xFF;  p[1] = positions[i] >> 8;
        p[2] = timesMs[i]  & 0xFF;  p[3] = timesMs[i]  >> 8;
        p[4] = speeds[i]   & 0xFF;  p[5] = speeds[i]   >> 8;
    }
    bus.syncWrite(Reg::TARGET_POS, 6, ids, dataArr, count);
}
