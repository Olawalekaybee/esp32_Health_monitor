#include "comms/comms_espnow.h"
#include "config.h"
#include <WiFi.h>
#include <esp_now.h>
#include <esp_arduino_version.h>
#include <Arduino.h>

static AckPacketCallback userAckCallback = nullptr;

// ESP-NOW's send callback runs from Wi-Fi's internal task context —
// keep it tiny, no blocking calls.
static void onDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
    if (status != ESP_NOW_SEND_SUCCESS) {
        Serial.println("[espnow] send failed");
    }
}

static void handleAckRecv(const uint8_t *data, int len) {
    if (len != sizeof(CydAckPacket)) {
        Serial.printf("[espnow] WARNING: got %d bytes, expected %d for CydAckPacket\n",
                      len, (int)sizeof(CydAckPacket));
        return;
    }
    if (userAckCallback) {
        CydAckPacket ack;
        memcpy(&ack, data, sizeof(CydAckPacket));
        userAckCallback(ack);
    }
}

// arduino-esp32 changed the ESP-NOW receive callback signature between
// major versions (core 3.x takes an esp_now_recv_info_t*, core 2.x
// takes a raw MAC address). Guard against whichever gets resolved.
#if ESP_ARDUINO_VERSION >= ESP_ARDUINO_VERSION_VAL(3, 0, 0)
static void onDataRecv(const esp_now_recv_info_t *info, const uint8_t *data, int len) {
    handleAckRecv(data, len);
}
#else
static void onDataRecv(const uint8_t *mac_addr, const uint8_t *data, int len) {
    handleAckRecv(data, len);
}
#endif

void espnow_comms_init(AckPacketCallback onAck) {
    userAckCallback = onAck;

    WiFi.mode(WIFI_STA);
    WiFi.disconnect();

    if (esp_now_init() != ESP_OK) {
        Serial.println("[espnow] FATAL: esp_now_init failed");
        return;
    }
    esp_now_register_send_cb(onDataSent);
    esp_now_register_recv_cb(onDataRecv);

    esp_now_peer_info_t peer = {};
    memcpy(peer.peer_addr, ESPNOW_PEER_MAC, 6);
    peer.channel = 0;
    peer.encrypt = false;

    if (esp_now_add_peer(&peer) != ESP_OK) {
        Serial.println("[espnow] FATAL: failed to add peer");
        return;
    }

    Serial.print("[espnow] Ready. This board's MAC: ");
    Serial.println(WiFi.macAddress());
    Serial.println("[espnow] Set that MAC as ESPNOW_PEER_MAC on the CYD if not using broadcast.");
}

void espnow_comms_send(const SensorHealthPacket &pkt) {
    esp_err_t result = esp_now_send(ESPNOW_PEER_MAC,
                                     reinterpret_cast<const uint8_t*>(&pkt),
                                     sizeof(pkt));
    if (result != ESP_OK) {
        Serial.println("[espnow] send failed to queue");
    }
}
