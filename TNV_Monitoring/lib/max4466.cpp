#include <Arduino.h>

void setup() {
    Serial.begin(115200);
    // Initialize ESP-NOW
    pinMode(34, INPUT); // Example pin for input
}

void loop() {
    // Main loop code
    Serial.println(analogRead(34)); // Read from pin 34
    delay(10); // Delay for readability
}