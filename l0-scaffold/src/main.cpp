#include <Arduino.h>

// Layer 0: proves the toolchain, board target, and upload path work.
// No application logic yet — that starts in Layer 1.

void setup() {
    Serial.begin(115200);
    delay(500);
    Serial.println("=== Layer 0 scaffold boot OK ===");
    pinMode(2, OUTPUT);
}

void loop() {
    digitalWrite(2, !digitalRead(2));
    Serial.println("heartbeat");
    delay(1000);
}
