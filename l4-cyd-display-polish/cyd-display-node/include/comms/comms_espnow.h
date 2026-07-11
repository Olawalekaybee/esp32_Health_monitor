#pragma once
#include "comms_packet.h"
#include "comms/comms_callback.h"
// ============================================================
// comms_espnow.h — ESP-NOW transport, CYD side. Proven working on
// real hardware.
// ============================================================

// Starts WiFi in STA mode, initializes ESP-NOW, and registers the
// receive callback for incoming SensorHealthPacket data. Call once
// from setup().
void espnow_comms_init(SensorPacketCallback onPacket);

// Sends one CydAckPacket back to main-node.
void espnow_comms_send_ack(const CydAckPacket &ack);
