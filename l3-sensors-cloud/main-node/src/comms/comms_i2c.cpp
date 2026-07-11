#include "comms/comms_i2c.h"
#include "pins.h"
#include <Wire.h>
#include <Arduino.h>

void i2c_comms_init() {
    Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL, 100000);
    Serial.println("[i2c] I2C master ready");
}

void i2c_comms_send(const SensorHealthPacket &pkt) {
    Wire.beginTransmission(I2C_SLAVE_ADDRESS);
    Wire.write(reinterpret_cast<const uint8_t*>(&pkt), sizeof(pkt));
    uint8_t result = Wire.endTransmission();
    if (result != 0) {
        Serial.printf("[i2c] write failed, error %d\n", result);
    }
}

bool i2c_comms_read_ack(CydAckPacket &out) {
    size_t received = Wire.requestFrom((uint8_t)I2C_SLAVE_ADDRESS, (uint8_t)sizeof(CydAckPacket));
    if (received != sizeof(CydAckPacket)) {
        return false; // CYD not ready yet, or bus error — not fatal
    }

    uint8_t buffer[sizeof(CydAckPacket)];
    for (size_t i = 0; i < sizeof(CydAckPacket); i++) {
        buffer[i] = Wire.read();
    }
    memcpy(&out, buffer, sizeof(CydAckPacket));
    return true;
}
