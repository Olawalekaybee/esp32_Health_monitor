#include "sensor_task.h"
#include "pins.h"
#include "config.h"

void sensorTask(void *pvParameters) {
    pinMode(PIN_SENSOR_ANALOG, INPUT);
    for (;;) {
        SensorSample sample;
        sample.timestamp_ms = millis();
        int raw = analogRead(PIN_SENSOR_ANALOG);
        sample.value = raw * (3.3f / 4095.0f);
        sample.read_ok = (raw >= 0 && raw <= 4095);
        // xQueueOverwrite, not xQueueSend: sensorQueue is a length-1
        // mailbox (every consumer uses xQueuePeek, nothing ever drains
        // it with xQueueReceive), so this always keeps just the latest
        // sample rather than filling up and dropping future ones.
        xQueueOverwrite(sensorQueue, &sample);
        vTaskDelay(pdMS_TO_TICKS(SENSOR_READ_INTERVAL_MS));
    }
}