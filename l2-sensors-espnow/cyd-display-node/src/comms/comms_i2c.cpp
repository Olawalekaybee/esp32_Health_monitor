#include "comms/comms_i2c.h"
#include "pins.h"
#include <Wire.h>
#include <Arduino.h>

static SensorPacketCallback userCallback = nullptr;
static volatile bool newData = false;
static SensorHealthPacket receivedPacket;
static CydAckPacket currentAck = {};

// Wire.onReceive runs in interrupt-ish context — keep it fast: just
// copy bytes and set a flag. userCallback() runs later, from
// i2c_comms_poll(), called from the main loop.
static void onReceive(int bytes) {
    if (bytes != sizeof(SensorHealthPacket)) {
        while (Wire.available()) {
            Wire.read();
        }
        return;
    }

    uint8_t buffer[sizeof(SensorHealthPacket)];
    for (size_t i = 0; i < sizeof(SensorHealthPacket); i++) {
        if (Wire.available()) {
            buffer[i] = Wire.read();
        }
    }
    memcpy(&receivedPacket, buffer, sizeof(SensorHealthPacket));
    newData = true;
}

// Called by the I2C hardware when main-node does Wire.requestFrom().
// Must be fast — just sends whatever ack data was last set.
static void onRequest() {
    Wire.write(reinterpret_cast<const uint8_t*>(&currentAck), sizeof(currentAck));
}

void i2c_comms_init(SensorPacketCallback onPacket) {
    userCallback = onPacket;
    Wire.begin((uint8_t)I2C_SLAVE_ADDRESS, I2C_SDA, I2C_SCL, 100000);
    Wire.onReceive(onReceive);
    Wire.onRequest(onRequest);
    Serial.println("[i2c] I2C slave ready");
}

void i2c_comms_poll() {
    if (newData) {
        newData = false;
        if (userCallback) {
            userCallback(receivedPacket);
        }
    }
}

void i2c_comms_set_ack(const CydAckPacket &ack) {
    currentAck = ack;
}
