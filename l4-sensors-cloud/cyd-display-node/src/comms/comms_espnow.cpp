#include "comms/comms_espnow.h"
#include <WiFi.h>
#include <esp_now.h>
#include <esp_arduino_version.h>
#include <Arduino.h>

static SensorPacketCallback userCallback = nullptr;
static const uint8_t broadcastMac[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
// main-node's actual WiFi/ESP-NOW MAC (confirmed from its own serial
// boot log) — used to send acks directly to it rather than
// broadcasting. Update this if main-node's hardware changes.
static const uint8_t mainNodeMac[6] = {0x14, 0xC1, 0x9F, 0x29, 0xFD, 0x34};
// main-node's Bluetooth MAC, for whenever the Bluetooth transport is
// actually implemented — currently a stub, see comms_bluetooth.h:
// 14:C1:9F:29:FD:35

static void handleRecv(const uint8_t *mac, const uint8_t *data, int len) {
    if (len != sizeof(SensorHealthPacket)) {
        Serial.printf("[espnow] WARNING: got %d bytes, expected %d — struct mismatch with main-node?\n",
                      len, (int)sizeof(SensorHealthPacket));
        return;
    }

    if (userCallback) {
        SensorHealthPacket pkt;
        memcpy(&pkt, data, sizeof(SensorHealthPacket));
        userCallback(pkt);
    }
}

#if ESP_ARDUINO_VERSION >= ESP_ARDUINO_VERSION_VAL(3, 0, 0)
static void onDataRecv(const esp_now_recv_info_t *info, const uint8_t *data, int len) {
    handleRecv(info->src_addr, data, len);
}
#else
static void onDataRecv(const uint8_t *mac_addr, const uint8_t *data, int len) {
    handleRecv(mac_addr, data, len);
}
#endif

void espnow_comms_init(SensorPacketCallback onPacket) {
    userCallback = onPacket;

    WiFi.mode(WIFI_STA);
    WiFi.disconnect();

    Serial.print("[espnow] This board's MAC: ");
    Serial.println(WiFi.macAddress());

    if (esp_now_init() != ESP_OK) {
        Serial.println("[espnow] FATAL: esp_now_init failed");
        return;
    }
    esp_now_register_recv_cb(onDataRecv);

    esp_now_peer_info_t broadcastPeer = {};
    memcpy(broadcastPeer.peer_addr, broadcastMac, 6);
    broadcastPeer.channel = 0;
    broadcastPeer.encrypt = false;
    esp_now_add_peer(&broadcastPeer);

    esp_now_peer_info_t mainNodePeer = {};
    memcpy(mainNodePeer.peer_addr, mainNodeMac, 6);
    mainNodePeer.channel = 0;
    mainNodePeer.encrypt = false;
    esp_now_add_peer(&mainNodePeer);
}

void espnow_comms_send_ack(const CydAckPacket &ack) {
    esp_now_send(mainNodeMac, reinterpret_cast<const uint8_t*>(&ack), sizeof(ack));
}