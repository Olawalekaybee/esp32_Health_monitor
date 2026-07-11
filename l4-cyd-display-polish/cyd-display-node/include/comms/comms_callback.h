#pragma once
#include "comms_packet.h"
// ============================================================
// comms_callback.h — the one thing every transport module agrees
// on, CYD side. Each transport calls this when a SensorHealthPacket
// arrives from main-node.
// ============================================================
typedef void (*SensorPacketCallback)(const SensorHealthPacket &pkt);
