#pragma once
#include "comms_packet.h"
#include "comms/comms_callback.h"
// ============================================================
// comms_bluetooth.h — BLE transport, CYD side (BLE CLIENT).
// Real, complete code (not a stub) — see main-node's comms_bluetooth.h
// for the ESP32-S3 Classic-Bluetooth limitation that led to using BLE
// on both sides instead. Untested on real hardware.
//
// Scans for main-node's BLE service, connects, subscribes to sensor
// data via NOTIFY, and WRITEs acks back.
// ============================================================

void bluetooth_comms_init(SensorPacketCallback onPacket);
void bluetooth_comms_send_ack(const CydAckPacket &ack);