#include <Arduino.h>
#include <esp_now.h>
#include <WiFi.h>
#include <Wire.h>
#include <U8g2lib.h>

#define LED 13
#define BUZZER 12
#define VIBSIG 34

// MAC Adress
uint8_t broadcastAddress[] = {0xEC, 0xE3, 0x34, 0x21, 0x91, 0x74};
// Sound Device MAC Address

// OLED 128x32 I2C setup
U8G2_SSD1306_128X32_UNIVISION_F_HW_I2C u8g2(U8G2_R0, /* reset=*/ U8X8_PIN_NONE);


char line[20] = "Vibration Monitor";
// Structure to receive dataec:e3:34:21:91:74
typedef struct struct_message {
  int id;
  u8_t data_id;
  float data;
} struct_message;

// Create a struct_message called myData
struct_message myData;

// Peer information
esp_now_peer_info_t peerInfo;

void SendData() {
    // Send data
    myData.id = 1; 
    myData.data_id = random(0, 255); 
    myData.data = random(0, 100) / 10.0;
    digitalWrite(LED, HIGH);
    // Send the data
    esp_err_t result = esp_now_send(broadcastAddress, (uint8_t *) &myData, sizeof(myData));

    if (result == ESP_OK) {
        Serial.println("Data sent!");
        Serial.printf("ID: %d\tData ID: %d\tData: %.2f\n", myData.id, myData.data_id, myData.data);
    } else {
        Serial.println("Error sending data");
    }
    delay(10);
}

void ReadData() {
    // Simulate reading data from a sensor
    char buffer[20];
    myData.data = analogRead(VIBSIG)/4096.0 *3.3; // Convert to voltage
    snprintf(buffer, sizeof(buffer), "Voltage: %.2f", myData.data);
    Serial.println(buffer);
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_ncenB08_tr);

    u8g2.drawStr(0, 10, line);
    u8g2.drawStr(0, 25, buffer);
    u8g2.sendBuffer();
}

// Callback when data is sent
void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
  Serial.print("Send Status: ");
  Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Success" : "Fail");
}

void setup() {
  Serial.begin(115200);
  WiFi.mode(WIFI_STA);
  Serial.println("Vibration Monitoring Device");
  Serial.println("ESP-NOW Sender 1 started");
  analogReadResolution(12);
  pinMode(LED, OUTPUT);
  pinMode(BUZZER, OUTPUT);
  pinMode(VIBSIG, INPUT);
  u8g2.begin();
  // Initialize ESP-NOW
  if (esp_now_init() != ESP_OK) {
    Serial.println("Error initializing ESP-NOW");
    return;
  }
  esp_now_register_send_cb(OnDataSent);

  // Set up peer information
  memcpy(peerInfo.peer_addr, broadcastAddress, 6);
  peerInfo.channel = 0; // Use current channel
  peerInfo.encrypt = false;

  // Add peer
  if (esp_now_add_peer(&peerInfo) != ESP_OK) {
    Serial.println("Failed to add peer");
    return;
  }
}

void loop(){
    ReadData();
    //SendData();
    digitalWrite(LED, LOW);
    delay(10);
}