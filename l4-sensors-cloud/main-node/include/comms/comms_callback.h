#pragma once
#include "comms_packet.h"
// ============================================================
// comms_callback.h — the one thing every transport module agrees
// on. Each transport (ESP-NOW, I2C, Bluetooth...) receives an ack
// from the CYD however it likes, then calls this callback. main.cpp
// registers ONE callback and doesn't care which transport(s)
// actually delivered it.
// ============================================================
typedef void (*AckPacketCallback)(const CydAckPacket &pkt);
