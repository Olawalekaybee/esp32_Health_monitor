#include "comms_task.h"
#include "config.h"
#include "comms_packet.h"

// --- Transport selection: flip to 1/0. Choose at most 2. ---
#define USE_ESPNOW    0
#define USE_I2C       1
#define USE_BLUETOOTH 0

#if (USE_ESPNOW + USE_I2C + USE_BLUETOOTH) > 2
    #error "Choose at most 2 transports (USE_ESPNOW / USE_I2C / USE_BLUETOOTH)"
#endif
#if (USE_ESPNOW + USE_I2C + USE_BLUETOOTH) == 0
    #error "Choose at least 1 transport (USE_ESPNOW / USE_I2C / USE_BLUETOOTH)"
#endif

#if USE_ESPNOW
    #include "comms/comms_espnow.h"
#endif
#if USE_I2C
    #include "comms/comms_i2c.h"
#endif
#if USE_BLUETOOTH
    #include "comms/comms_bluetooth.h"
#endif

// Last ack received from the CYD, across whichever transport(s) sent
// it — used only for logging here; a later layer could feed this
// into SystemStatus.cyd_link_ok.
static volatile bool lastAckLinkOk = false;
static volatile uint32_t lastAckMillis = 0;

static void onAckReceived(const CydAckPacket &ack) {
    lastAckLinkOk = (ack.link_ok != 0);
    lastAckMillis = millis();
    Serial.printf("[comms_task] Ack received from CYD: uptime=%lums, link_ok=%d\n",
                  ack.cyd_uptime_ms, ack.link_ok);
}

void commsTask(void *pvParameters) {
#if USE_ESPNOW
    espnow_comms_init(onAckReceived);
#endif
#if USE_I2C
    i2c_comms_init();
#endif
#if USE_BLUETOOTH
    bluetooth_comms_init(onAckReceived);
#endif

    SensorSample sample;
    HealthReport health;

    for (;;) {
        bool haveSample = (xQueuePeek(sensorQueue, &sample, 0) == pdTRUE);
        bool haveHealth = (xQueuePeek(healthQueue, &health, 0) == pdTRUE);

        if (haveSample && haveHealth) {
            SensorHealthPacket packet = {};
            packet.timestamp_ms          = sample.timestamp_ms;
            packet.dht_temperature_c     = sample.dht_temperature_c;
            packet.dht_humidity_pct      = sample.dht_humidity_pct;
            packet.dht_read_ok           = sample.dht_read_ok ? 1 : 0;
            packet.ds18b20_temperature_c = sample.ds18b20_temperature_c;
            packet.ds18b20_read_ok       = sample.ds18b20_read_ok ? 1 : 0;
            packet.health_status         = static_cast<uint8_t>(health.status);
            strncpy(packet.health_reason, health.reason, sizeof(packet.health_reason) - 1);

#if USE_ESPNOW
            espnow_comms_send(packet);
            // Ack for ESP-NOW arrives asynchronously via onAckReceived,
            // registered above — nothing more to do here.
#endif
#if USE_I2C
            i2c_comms_send(packet);
            // I2C's read-back is master-initiated (no async callback),
            // so poll for the ack right after sending.
            CydAckPacket ack;
            if (i2c_comms_read_ack(ack)) {
                onAckReceived(ack);
            }
#endif
#if USE_BLUETOOTH
            bluetooth_comms_send(packet);
#endif
        }

        vTaskDelay(pdMS_TO_TICKS(COMMS_SEND_INTERVAL_MS));
    }
}
