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

// pins.h's SCREEN_WIDTH/SCREEN_HEIGHT (240x320) are the panel's
// PRE-rotation native dimensions — correct for lv_tft_espi_create()
// below, which is rotated 90° afterward. Touch coordinates need the
// POST-rotation LOGICAL dimensions instead (320x240) — using
// SCREEN_WIDTH/SCREEN_HEIGHT directly in touchscreen_read() was
// exactly backwards, and the actual root cause of every "calibration"
// mismatch traced through several rounds of debugging: X was being
// computed and clamped against 240, not 320.
#define TOUCH_LOGICAL_WIDTH  320
#define TOUCH_LOGICAL_HEIGHT 240
uint32_t draw_buf[DRAW_BUF_SIZE / 4];

// --- Shared comms state, fed by whichever transport(s) are active ---
static CommsPacket latest;
static volatile bool havePacket = false;
static volatile uint32_t lastPacketMillis = 0;

// Layer 6 (touch/settings): state set by ui.cpp's touch handlers, read
// here when building the outgoing ack. ui.cpp owns rendering and
// touch; this file owns building the ack packet — these two functions
// are the deliberate, narrow crossing point between them, declared in
// ui.h and implemented here rather than in ui.cpp.
static uint8_t g_requestedCloudBackend = 0; // 0=Google Sheets, matches CloudBackendId on main-node
static uint32_t g_forceSyncUntilMs = 0;      // ack keeps force_sync_requested=1 until millis() passes this

void app_request_cloud_backend(uint8_t backend_id) {
    g_requestedCloudBackend = backend_id;
    Serial.printf("[main] Cloud backend requested: %d\n", backend_id);
}

void app_request_force_sync() {
    // Held "on" for 2 seconds rather than one single ack transmission —
    // main-node polls for the ack roughly once a second (COMMS_SEND_INTERVAL_MS
    // on that side), so a single-shot flag risks landing between polls
    // and being missed entirely. 2 seconds gives it a couple of chances.
    g_forceSyncUntilMs = millis() + 2000;
    Serial.println("[main] Force sync requested");
}

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

// Touch input for the settings/detail screens and card/badge/banner
// taps (Layer 6). Matches the check used in a known-working minimal
// reference sketch — touched() alone, without also requiring
// tirqTouched() (an IRQ-pin check that adds a dependency on IRQ
// wiring/timing behaving exactly right, which the reference didn't
// need to get working touch).
//
// Calibrated against this specific physical panel via a four-corner
// raw-coordinate test (2026-07-13):
//   top-left     raw=(3745, 520)    top-right    raw=(433, 659)
//   bottom-left  raw=(3710, 3614)   bottom-right raw=(369, 3437)
// X is inverted (raw X decreases left-to-right) relative to the
// generic reference sketch's assumption — a real difference on this
// unit, not a guess. Edge values below are the average of each pair.
void touchscreen_read(lv_indev_t *indev, lv_indev_data_t *data) {
    // Resistive touch panels commonly read noisy/unsettled values for
    // the first few ms right at initial contact, before the resistive
    // layers fully settle against each other — this is a real, well
    // documented characteristic of the hardware, not a calibration
    // bug.
    //
    // An earlier version of this fix discarded the first sample and
    // waited for LVGL's *next* poll cycle (~30ms later) to trust a
    // reading — but a tap brief enough to release before that next
    // poll happens would never get a valid press reported at all,
    // silently dropping the tap entirely rather than just delaying
    // it. That's a real regression this caused: a short, light tap
    // (e.g. on a small text link) is exactly the kind of touch likely
    // to be that brief. Settling the reading here, within the same
    // call, guarantees a real reading is used regardless of how brief
    // the physical contact is or how it lines up with the poll timer.
    if (touchscreen.touched()) {
        delay(3); // let the resistive layers settle before trusting the reading
        TS_Point p = touchscreen.getPoint();
        // X-axis recalibrated again (2026-07-13, second pass) using
        // BOTH live reference points together — gear icon (raw~3505,
        // should map to ~300) and "Back" label (raw~532, should map
        // to ~35) — verified by direct computation to land exactly
        // on both: forward-checked in Python using Arduino's actual
        // integer map() semantics before writing this, not just
        // algebra on paper.
        int16_t x = map(p.x, 150, 3729, 1, TOUCH_LOGICAL_WIDTH);
        int16_t y = map(p.y, 590, 3526, 1, TOUCH_LOGICAL_HEIGHT);
        // map() extrapolates rather than clips when the raw reading
        // falls slightly outside the calibrated min/max (easy to hit
        // right at a screen edge) — clamp so LVGL never gets a
        // coordinate outside the actual screen.
        data->point.x = constrain(x, 1, TOUCH_LOGICAL_WIDTH);
        data->point.y = constrain(y, 1, TOUCH_LOGICAL_HEIGHT);
        data->state = LV_INDEV_STATE_PRESSED;

        Serial.printf("[touch] raw=(%d,%d) mapped=(%d,%d)\n",
                      p.x, p.y, data->point.x, data->point.y);
    } else {
        data->state = LV_INDEV_STATE_RELEASED;
    }
}

// Root-caused against a known-working minimal reference sketch: that
// sketch initializes the display (tft.init()) BEFORE touch's SPIClass
// touches the same physical VSPI bus, and only calls lv_init() after
// both. Our previous order had lv_init() first, then touch's SPI.begin(),
// then the display last — touch claiming the shared bus before the
// display had configured it. Reordering to match the proven-working
// sequence: display first, touch second.
#define ENABLE_TOUCH 1

void setup() {
    Serial.begin(115200);
    delay(500);
    Serial.println("\n=== CYD Display Node boot ===");
    Serial.println("=== BUILD MARKER: calibration v3 (150,3729) ===");

    // Even with touch disabled, its CS pin sits on the same SPI bus as
    // the display. Left floating, it can partially respond and corrupt
    // the display's SPI traffic — explicitly deselecting it first.
    pinMode(XPT2046_CS, OUTPUT);
    digitalWrite(XPT2046_CS, HIGH);

    // --- LVGL core (no display/indev registered yet) ---
    lv_init();
    lv_log_register_print_cb(log_print);

    // --- Display FIRST — must claim and configure the shared VSPI bus
    //     before touch's SPIClass touches it at all. ---
    lv_display_t *disp = lv_tft_espi_create(SCREEN_WIDTH, SCREEN_HEIGHT, draw_buf, sizeof(draw_buf));
    lv_display_set_rotation(disp, LV_DISPLAY_ROTATION_90);
    // Disable LVGL's default theme entirely — without this it silently
    // re-applies a white background to every object as it's created,
    // overriding ui.cpp's custom dark styling.
    lv_display_set_theme(disp, NULL);

#if ENABLE_TOUCH
    // --- Touch SECOND, only after the display above is fully set up ---
    touchscreenSPI.begin(XPT2046_CLK, XPT2046_MISO, XPT2046_MOSI, XPT2046_CS);
    touchscreen.begin(touchscreenSPI);
    // Matches lv_display_set_rotation's LV_DISPLAY_ROTATION_90 above —
    // previously this was set to 2 while the display used rotation 1
    // (90°), a mismatch that wouldn't hang anything but could still
    // have made taps land at the wrong screen location.
    touchscreen.setRotation(1);

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
        ack.requested_cloud_backend = g_requestedCloudBackend;
        ack.force_sync_requested = (millis() < g_forceSyncUntilMs) ? 1 : 0;

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