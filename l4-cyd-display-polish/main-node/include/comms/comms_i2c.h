#pragma once
#include "comms_packet.h"
// ============================================================
// comms_i2c.h — I2C transport, main-node side (I2C MASTER).
// Real, standard implementation (Wire master write + requestFrom
// read) — untested on real hardware for this project, since it's
// new alongside this feature. Test before relying on it.
// ============================================================

// Starts Wire in master mode. Call once from setup().
void i2c_comms_init();

// Writes one SensorHealthPacket to the CYD (I2C slave).
void i2c_comms_send(const SensorHealthPacket &pkt);

// Requests a CydAckPacket from the CYD and reads it if available.
// Returns true and fills `out` if a full packet was read; false
// otherwise (e.g. CYD not yet ready, bus error).
bool i2c_comms_read_ack(CydAckPacket &out);
