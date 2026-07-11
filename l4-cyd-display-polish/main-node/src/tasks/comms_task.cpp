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
// it. Fed into SystemStatus.cyd_link_ok below (with a staleness
// check), so comms_task itself can report whether its own link back
// to the CYD is actually healthy.
static volatile bool lastAckLinkOk = false;
static volatile uint32_t lastAckMillis = 0;

static void onAckReceived(const CydAckPacket &ack) {
    lastAckLinkOk = (ack.link_ok != 0);
    lastAckMillis = millis();
    Serial.printf("[comms_task] Ack received from CYD: uptime=%lums, link_ok=%d\n",
                  ack.cyd_uptime_ms, ack.link_ok);
}

// Same read-modify-write pattern network_task/storage_task use — see
// network_task.cpp's publishNetworkStatus() comment for why this is
// safe despite not being a true atomic multi-field update.
static void publishLinkStatus(bool linkOk) {
    SystemStatus status = {};
    xQueuePeek(statusQueue, &status, 0);
    status.cyd_link_ok = linkOk;
    xQueueOverwrite(statusQueue, &status);
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

        // A previous ack that's gone quiet for too long is worth
        // treating as "link down" rather than trusting a stale
        // link_ok=true forever — mirrors the CYD side's own linkStale
        // logic (see cyd-display-node/src/main.cpp) so both boards
        // agree on what "stale" means.
        bool linkStale = (millis() - lastAckMillis) > CYD_LINK_STALE_MS;
        bool effectiveLinkOk = lastAckLinkOk && !linkStale;
        publishLinkStatus(effectiveLinkOk);

        if (haveSample && haveHealth) {
            SystemStatus status = {};
            xQueuePeek(statusQueue, &status, 0);

            SensorHealthPacket packet = {};
            packet.timestamp_ms          = sample.timestamp_ms;
            packet.dht_temperature_c     = sample.dht_temperature_c;
            packet.dht_humidity_pct      = sample.dht_humidity_pct;
            packet.dht_read_ok           = sample.dht_read_ok ? 1 : 0;
            packet.ds18b20_temperature_c = sample.ds18b20_temperature_c;
            packet.ds18b20_read_ok       = sample.ds18b20_read_ok ? 1 : 0;
            packet.health_status         = static_cast<uint8_t>(health.status);
            strncpy(packet.health_reason, health.reason, sizeof(packet.health_reason) - 1);
            packet.wifi_connected        = status.wifi_connected ? 1 : 0;
            packet.sd_card_ok            = status.sd_card_ok ? 1 : 0;
            packet.cloud_sync_ok         = status.cloud_sync_ok ? 1 : 0;

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
