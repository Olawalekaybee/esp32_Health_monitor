#pragma once
#include "comms_packet.h"
#include "comms/comms_callback.h"
// ============================================================
// comms_i2c.h — I2C transport, CYD side (I2C SLAVE).
// Real, standard implementation (Wire.onReceive + Wire.onRequest) —
// untested on real hardware for this project. Test before relying
// on it.
// ============================================================

// Starts Wire in slave mode at I2C_SLAVE_ADDRESS (pins.h) and
// registers the receive/request callbacks. Call once from setup().
void i2c_comms_init(SensorPacketCallback onPacket);

// Delivers any packet received since the last call, from the main
// loop's context rather than I2C's own callback context — call this
// once per loop() iteration if i2c_comms_init() was used.
void i2c_comms_poll();

// Sets the ack data that will be sent the next time main-node reads
// from us (Wire.requestFrom). Call this whenever the ack content
// should change (e.g. once per loop, with a fresh uptime value).
void i2c_comms_set_ack(const CydAckPacket &ack);
