#include "comms/comms_bluetooth.h"
#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEClient.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>

// Must match main-node's comms_bluetooth.cpp exactly.
#define BLE_SERVICE_UUID     "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define BLE_SENSOR_CHAR_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a8"
#define BLE_ACK_CHAR_UUID    "d0e93f2a-1e0a-4d1a-9d6a-1a6b3e8c9f10"

static SensorPacketCallback userCallback = nullptr;
static BLERemoteCharacteristic *ackChar = nullptr;
static BLEClient *client = nullptr;
static volatile bool connected = false;
static BLEAdvertisedDevice *targetDevice = nullptr;

static void notifyCallback(BLERemoteCharacteristic *ch, uint8_t *data, size_t length, bool isNotify) {
    if (length != sizeof(SensorHealthPacket)) {
        Serial.printf("[bluetooth] WARNING: got %d bytes, expected %d — struct mismatch with main-node?\n",
                      (int)length, (int)sizeof(SensorHealthPacket));
        return;
    }
    if (userCallback) {
        SensorHealthPacket pkt;
        memcpy(&pkt, data, sizeof(pkt));
        userCallback(pkt);
    }
}

class AdvertisedDeviceCallbacks : public BLEAdvertisedDeviceCallbacks {
    void onResult(BLEAdvertisedDevice advertisedDevice) override {
        if (advertisedDevice.haveServiceUUID() &&
            advertisedDevice.isAdvertisingService(BLEUUID(BLE_SERVICE_UUID))) {
            BLEDevice::getScan()->stop();
            targetDevice = new BLEAdvertisedDevice(advertisedDevice);
        }
    }
};

static bool connectToServer() {
    Serial.println("[bluetooth] connecting to main-node...");
    client = BLEDevice::createClient();

    if (!client->connect(targetDevice)) {
        Serial.println("[bluetooth] connect failed");
        return false;
    }

    // Request a larger MTU than the 23-byte BLE default — our
    // SensorHealthPacket is ~51 bytes and would otherwise get
    // truncated by the stack without any obvious error. 200 gives
    // comfortable headroom for both current packets and future growth.
    client->setMTU(200);

    BLERemoteService *service = client->getService(BLE_SERVICE_UUID);
    if (service == nullptr) {
        Serial.println("[bluetooth] service UUID not found on connected device");
        client->disconnect();
        return false;
    }

    BLERemoteCharacteristic *sensorChar = service->getCharacteristic(BLE_SENSOR_CHAR_UUID);
    if (sensorChar != nullptr && sensorChar->canNotify()) {
        sensorChar->registerForNotify(notifyCallback);
    } else {
        Serial.println("[bluetooth] WARNING: sensor characteristic not found or can't notify");
    }

    ackChar = service->getCharacteristic(BLE_ACK_CHAR_UUID);
    if (ackChar == nullptr) {
        Serial.println("[bluetooth] WARNING: ack characteristic not found");
    }

    Serial.println("[bluetooth] connected to main-node");
    return true;
}

void bluetooth_comms_init(SensorPacketCallback onPacket) {
    userCallback = onPacket;

    BLEDevice::init("ESP32-HealthMonitor-CYD");
    BLEScan *scan = BLEDevice::getScan();
    scan->setAdvertisedDeviceCallbacks(new AdvertisedDeviceCallbacks());
    scan->setInterval(1349);
    scan->setWindow(449);
    scan->setActiveScan(true);
    scan->start(5, false); // scan for 5s; onResult() stops it early if found

    Serial.println("[bluetooth] scanning for main-node...");
}

void bluetooth_comms_send_ack(const CydAckPacket &ack) {
    // Not connected yet, but we've spotted the server advertising —
    // try to connect now rather than waiting for a dedicated poll loop.
    if (!connected && targetDevice != nullptr) {
        connected = connectToServer();
    }

    if (connected && client != nullptr && !client->isConnected()) {
        // Connection dropped since last check.
        connected = false;
        Serial.println("[bluetooth] connection lost, will retry");
    }

    if (connected && ackChar != nullptr) {
        ackChar->writeValue((uint8_t *)&ack, sizeof(ack), false);
    }
}