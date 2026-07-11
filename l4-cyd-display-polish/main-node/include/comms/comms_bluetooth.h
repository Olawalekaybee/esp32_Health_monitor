#pragma once
#include "comms_packet.h"
#include "comms/comms_callback.h"
// ============================================================
// comms_bluetooth.h — BLE transport, main-node side (BLE SERVER).
//
// Role split: main-node advertises a BLE service and pushes sensor
// data via NOTIFY; the CYD connects as a client and WRITEs acks back.
// ============================================================

void bluetooth_comms_init(AckPacketCallback onAck);
void bluetooth_comms_send(const SensorHealthPacket &pkt);