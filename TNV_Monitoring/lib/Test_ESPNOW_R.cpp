#include <Arduino.h>
#include <esp_now.h>
#include <WiFi.h>
#include <Wire.h>
#include <U8g2lib.h>
#define LED 12
#define BUZZER 13

// OLED 128x32 I2C setup
U8G2_SSD1306_128X32_UNIVISION_F_HW_I2C u8g2(U8G2_R0, /* reset=*/ U8X8_PIN_NONE);

// Structure for incoming data
typedef struct struct_message {
  int id;
  float data;
} struct_message;

struct_message myData;
struct_message receivedData[3]; // Array to store up to 3 boards' data
bool flag = false;

// Callback when data is received
void OnDataRecv(const uint8_t *mac_addr, const uint8_t *incomingData, int len) {
    memcpy(&myData, incomingData, sizeof(myData));

    // Store in array based on board ID (assuming IDs start from 1)
    if (myData.id >= 1 && myData.id <= 3) {
        receivedData[myData.id - 1] = myData;
    }

    // Debug print
    char macStr[18];
    snprintf(macStr, sizeof(macStr), "%02X:%02X:%02X:%02X:%02X:%02X",
        mac_addr[0], mac_addr[1], mac_addr[2],
        mac_addr[3], mac_addr[4], mac_addr[5]);

    Serial.printf("From: %s | Board ID: %d | Data ID: %d | Data: %.2f\n",
                  macStr, myData.id, myData.data_id, myData.data);

    // Display on OLED
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_ncenB08_tr);

    // Create display strings
    char line1[20];
    char line2[20];

    snprintf(line1, sizeof(line1), "Board %d", myData.id);
    snprintf(line2, sizeof(line2), "Data: %.2f", myData.data);

    u8g2.drawStr(0, 10, line1);
    u8g2.drawStr(0, 25, line2);

    u8g2.sendBuffer();
    flag = true;
}

void setup() {
    Serial.begin(115200);

    // Init OLED
    u8g2.begin();
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_ncenB08_tr);
    u8g2.drawStr(0, 15, "Waiting for data...");
    u8g2.sendBuffer();

    // Init Wi-Fi
    WiFi.mode(WIFI_STA);

    // Init ESP-NOW
    if (esp_now_init() != ESP_OK) {
        Serial.println("Error initializing ESP-NOW");
        return;
    }
    pinMode(LED, OUTPUT);
    pinMode(BUZZER, OUTPUT);
    // Register receive callback
    esp_now_register_recv_cb(OnDataRecv);
}

void loop() {
    if (flag) {
        flag = false; // Reset flag after processing
        digitalWrite(LED, HIGH); // Turn on LED
        //digitalWrite(BUZZER, HIGH); // Turn on Buzzer
        delay(100);
        digitalWrite(LED, LOW); // Turn off LED
        //digitalWrite(BUZZER, LOW); // Turn off Buzzer
    }
}
