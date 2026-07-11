#pragma once
#include "comms_packet.h"
#include "comms/comms_callback.h"
// ============================================================
// comms_espnow.h — ESP-NOW transport, main-node side.
// Proven working on real hardware (this is the transport this
// project was originally built and tested against).
// ============================================================

// Starts WiFi in STA mode, initializes ESP-NOW, adds the CYD as a
// peer, and registers the receive callback for incoming CydAckPacket
// data. Call once from setup()/commsTask's startup.
void espnow_comms_init(AckPacketCallback onAck);

// Sends one SensorHealthPacket to the CYD. Non-blocking; failures
// are logged, not thrown.
void espnow_comms_send(const SensorHealthPacket &pkt);
