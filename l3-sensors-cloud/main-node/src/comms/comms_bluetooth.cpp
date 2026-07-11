#include "comms/comms_bluetooth.h"
#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

// Service/characteristic UUIDs — must match EXACTLY on both main-node
// and cyd-display-node, or the two will never find each other. These
// are arbitrary valid 128-bit UUIDs, not tied to any registry.
#define BLE_SERVICE_UUID     "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define BLE_SENSOR_CHAR_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a8" // main-node -> CYD (NOTIFY)
#define BLE_ACK_CHAR_UUID    "d0e93f2a-1e0a-4d1a-9d6a-1a6b3e8c9f10" // CYD -> main-node (WRITE)

static AckPacketCallback userAckCallback = nullptr;
static BLECharacteristic *sensorChar = nullptr;
static volatile bool clientConnected = false;

class ServerCallbacks : public BLEServerCallbacks {
    void onConnect(BLEServer *server) override {
        clientConnected = true;
        Serial.println("[bluetooth] CYD connected");
    }
    void onDisconnect(BLEServer *server) override {
        clientConnected = false;
        Serial.println("[bluetooth] CYD disconnected — restarting advertising");
        // Without this, the server stops being discoverable after the
        // first disconnect and never reconnects.
        server->getAdvertising()->start();
    }
};

class AckCallbacks : public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic *ch) override {
        std::string value = ch->getValue();
        if (value.length() != sizeof(CydAckPacket)) {
            Serial.printf("[bluetooth] WARNING: got %d bytes, expected %d for CydAckPacket\n",
                          (int)value.length(), (int)sizeof(CydAckPacket));
            return;
        }
        if (userAckCallback) {
            CydAckPacket ack;
            memcpy(&ack, value.data(), sizeof(CydAckPacket));
            userAckCallback(ack);
        }
    }
};

void bluetooth_comms_init(AckPacketCallback onAck) {
    userAckCallback = onAck;

    BLEDevice::init("ESP32-HealthMonitor-MainNode");
    BLEServer *server = BLEDevice::createServer();
    server->setCallbacks(new ServerCallbacks());

    BLEService *service = server->createService(BLE_SERVICE_UUID);

    sensorChar = service->createCharacteristic(
        BLE_SENSOR_CHAR_UUID,
        BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY);
    // BLE2902 descriptor is required for NOTIFY to actually work — the
    // client writes to it to subscribe. Easy to forget and notifications
    // silently never arrive without it.
    sensorChar->addDescriptor(new BLE2902());

    BLECharacteristic *ackChar = service->createCharacteristic(
        BLE_ACK_CHAR_UUID,
        BLECharacteristic::PROPERTY_WRITE);
    ackChar->setCallbacks(new AckCallbacks());

    service->start();

    BLEAdvertising *advertising = BLEDevice::getAdvertising();
    advertising->addServiceUUID(BLE_SERVICE_UUID);
    advertising->setScanResponse(true);
    BLEDevice::startAdvertising();

    Serial.println("[bluetooth] BLE server advertising as 'ESP32-HealthMonitor-MainNode'");
}

void bluetooth_comms_send(const SensorHealthPacket &pkt) {
    if (!clientConnected || sensorChar == nullptr) {
        return; // nothing connected to send to yet
    }
    // SensorHealthPacket is ~51 bytes — exceeds BLE's default 20-byte
    // usable payload unless the client negotiates a larger MTU (see
    // the CYD-side client code, which requests one on connect). If
    // that negotiation didn't happen for some reason, this may get
    // silently truncated by the BLE stack rather than failing loudly.
    sensorChar->setValue((uint8_t *)&pkt, sizeof(pkt));
    sensorChar->notify();
}