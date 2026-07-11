/* ============================================================
   cyd-display-node — receives sensor + health data from main-node
   over one or more transports and shows it using LVGL (ui.cpp).
   Sends a lightweight ack back over the same link — a genuine
   two-way connection, not just one-way broadcast.

   Transport selection: flip the USE_* defines below. Choose at
   most 2 — enforced at compile time.
     - ESP-NOW  : proven working on real hardware.
     - I2C      : real implementation, untested on real hardware
                  for this project (no I2C main-node testing done yet).
     - Bluetooth: scaffold only — see comms/comms_bluetooth.h.

   Display + touch init boilerplate adapted from the proven working
   reference at:
   https://randomnerdtutorials.com/lvgl-cheap-yellow-display-esp32-2432s028r/
   ============================================================ */

#include <lvgl.h>
#include <TFT_eSPI.h>
#include <XPT2046_Touchscreen.h>
#include "pins.h"
#include "comms_packet.h"
#include "ui.h"

// --- Transport selection ---
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

SPIClass touchscreenSPI = SPIClass(VSPI);
XPT2046_Touchscreen touchscreen(XPT2046_CS, XPT2046_IRQ);

#define DRAW_BUF_SIZE (SCREEN_WIDTH * SCREEN_HEIGHT / 10 * (LV_COLOR_DEPTH / 8))
uint32_t draw_buf[DRAW_BUF_SIZE / 4];

// --- Shared comms state, fed by whichever transport(s) are active ---
static CommsPacket latest;
static volatile bool havePacket = false;
static volatile uint32_t lastPacketMillis = 0;

// The one callback every transport calls. Keep it fast — ESP-NOW's
// version runs in Wi-Fi's internal task context, so no LVGL calls here.
static void onSensorPacket(const SensorHealthPacket &pkt) {
    latest = pkt;
    havePacket = true;
    lastPacketMillis = millis();
    Serial.printf("[main] Packet received from main-node: DHT=%.1fC DS18B20=%.2fC status=%d\n",
                  pkt.dht_temperature_c, pkt.ds18b20_temperature_c, pkt.health_status);
}

// If logging is enabled, informs the user what's happening inside LVGL
void log_print(lv_log_level_t level, const char *buf) {
    LV_UNUSED(level);
    Serial.println(buf);
    Serial.flush();
}

// Touch input isn't used for any interactive widgets yet, but LVGL
// still needs a read callback registered for the indev to be valid.
void touchscreen_read(lv_indev_t *indev, lv_indev_data_t *data) {
    if (touchscreen.tirqTouched() && touchscreen.touched()) {
        TS_Point p = touchscreen.getPoint();
        data->point.x = map(p.x, 200, 3700, 1, SCREEN_WIDTH);
        data->point.y = map(p.y, 240, 3800, 1, SCREEN_HEIGHT);
        data->state = LV_INDEV_STATE_PRESSED;
    } else {
        data->state = LV_INDEV_STATE_RELEASED;
    }
}

// Touch temporarily disabled to isolate a hang — flip this to 1 once
// the display is confirmed working without it.
#define ENABLE_TOUCH 0

void setup() {
    Serial.begin(115200);
    delay(500);
    Serial.println("\n=== CYD Display Node boot ===");

    // Even with touch disabled, its CS pin sits on the same SPI bus as
    // the display. Left floating, it can partially respond and corrupt
    // the display's SPI traffic — explicitly deselecting it first.
    pinMode(XPT2046_CS, OUTPUT);
    digitalWrite(XPT2046_CS, HIGH);

    // --- LVGL ---
    lv_init();
    lv_log_register_print_cb(log_print);

#if ENABLE_TOUCH
    touchscreenSPI.begin(XPT2046_CLK, XPT2046_MISO, XPT2046_MOSI, XPT2046_CS);
    touchscreen.begin(touchscreenSPI);
    touchscreen.setRotation(2);
#endif

    // --- Display ---
    lv_display_t *disp = lv_tft_espi_create(SCREEN_WIDTH, SCREEN_HEIGHT, draw_buf, sizeof(draw_buf));
    lv_display_set_rotation(disp, LV_DISPLAY_ROTATION_90);
    // Disable LVGL's default theme entirely — without this it silently
    // re-applies a white background to every object as it's created,
    // overriding ui.cpp's custom dark styling.
    lv_display_set_theme(disp, NULL);

#if ENABLE_TOUCH
    lv_indev_t *indev = lv_indev_create();
    lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(indev, touchscreen_read);
#endif

    ui_create();

    // Force one screen flush now, so the dashboard actually appears even
    // if a transport's init below hangs — otherwise nothing draws to the
    // physical panel until loop() is reached.
    lv_task_handler();

    // --- Comms: bring up every enabled transport, all reporting to
    //     the same onSensorPacket() callback ---
#if USE_ESPNOW
    espnow_comms_init(onSensorPacket);
#endif
#if USE_I2C
    i2c_comms_init(onSensorPacket);
#endif
#if USE_BLUETOOTH
    bluetooth_comms_init(onSensorPacket);
#endif
}

void loop() {
    static uint32_t lastTick = 0;
    uint32_t now = millis();
    lv_tick_inc(now - lastTick);
    lastTick = now;

    lv_task_handler();

#if USE_I2C
    // I2C's own receive callback runs in a context where it's not
    // safe to do more work — this delivers any packet received since
    // the last poll from the safe main-loop context instead.
    i2c_comms_poll();
#endif

    static uint32_t lastRefresh = 0;
    if (millis() - lastRefresh >= 500) {   // matches DISPLAY_REFRESH_INTERVAL_MS on main-node
        bool linkStale = havePacket && (millis() - lastPacketMillis) > 10000; // no packet in 10s
        ui_update(latest, havePacket, linkStale);

        // "active" = we've received something recently. Sending our own
        // ack doesn't count — if main-node isn't there to hear it, this
        // should still show red, not green.
        const char *transportName =
#if USE_ESPNOW
            "ESP-NOW";
#elif USE_I2C
            "I2C";
#elif USE_BLUETOOTH
            "Bluetooth";
#else
            "none";
#endif
        ui_set_comms_status(transportName, havePacket && !linkStale);

        // --- Send an ack back, proving the link is genuinely two-way ---
        CydAckPacket ack;
        ack.cyd_uptime_ms = millis();
        ack.link_ok = havePacket ? 1 : 0;

#if USE_ESPNOW
        espnow_comms_send_ack(ack);
#endif
#if USE_I2C
        i2c_comms_set_ack(ack);
#endif
#if USE_BLUETOOTH
        bluetooth_comms_send_ack(ack);
#endif

        lastRefresh = millis();
    }

    delay(5);
}