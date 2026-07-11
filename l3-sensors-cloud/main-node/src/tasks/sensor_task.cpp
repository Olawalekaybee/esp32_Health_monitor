#include "sensor_task.h"
#include "pins.h"
#include "config.h"

#include <DHT.h>
#include <OneWire.h>
#include <DallasTemperature.h>

// --- DHT22 ---
static DHT dht(PIN_DHT22, DHT22);

// --- DS18B20 (OneWire bus, 1 sensor assumed for Layer 2) ---
static OneWire oneWire(PIN_DS18B20);
static DallasTemperature ds18b20(&oneWire);

void sensorTask(void *pvParameters) {
    dht.begin();
    ds18b20.begin();

    // Sanity check at boot: is a DS18B20 actually present on the bus?
    if (ds18b20.getDeviceCount() == 0) {
        Serial.println("[sensor_task] WARNING: no DS18B20 detected on OneWire bus");
    }

    for (;;) {
        SensorSample sample;
        sample.timestamp_ms = millis();

        // --- DHT22 read ---
        // NAN is the DHT library's failure signal — check for it explicitly
        // rather than trusting any specific numeric range.
        float h = dht.readHumidity();
        float t = dht.readTemperature(); // Celsius
        sample.dht_read_ok = !isnan(h) && !isnan(t);
        sample.dht_humidity_pct   = sample.dht_read_ok ? h : NAN;
        sample.dht_temperature_c  = sample.dht_read_ok ? t : NAN;

        // --- DS18B20 read ---
        ds18b20.requestTemperatures();
        float dsTemp = ds18b20.getTempCByIndex(0);
        // DEVICE_DISCONNECTED_C (-127.0) is the library's failure signal
        sample.ds18b20_read_ok = (dsTemp != DEVICE_DISCONNECTED_C);
        sample.ds18b20_temperature_c = sample.ds18b20_read_ok ? dsTemp : NAN;

        if (!sample.dht_read_ok) {
            Serial.println("[sensor_task] DHT22 read failed");
        }
        if (!sample.ds18b20_read_ok) {
            Serial.println("[sensor_task] DS18B20 read failed");
        }

        // xQueueOverwrite, not xQueueSend: sensorQueue is a length-1
        // mailbox (every consumer uses xQueuePeek), so this always keeps
        // just the latest sample rather than filling up and dropping
        // future ones.
        xQueueOverwrite(sensorQueue, &sample);

        // DHT22 needs >=2s between reads; SENSOR_READ_INTERVAL_MS
        // in config.h is set to 2000ms specifically for this reason.
        vTaskDelay(pdMS_TO_TICKS(SENSOR_READ_INTERVAL_MS));
    }
}
