#include <Arduino.h>
#include <esp_now.h>
#include <WiFi.h>

// MAC Adress
uint8_t broadcastAddress[] = {0xEC, 0xE3, 0x34, 0x21, 0x91, 0x74};

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

// Callback when data is sent
void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
  Serial.print("Send Status: ");
  Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Success" : "Fail");
}

void setup() {
  Serial.begin(115200);
  WiFi.mode(WIFI_STA);
  Serial.println("ESP-NOW Sender 1 started");

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
    // Send data
    myData.id = 1; // Change for different sender IDs
    myData.data_id = random(0, 255); // for verification
    myData.data = random(0, 100) / 10.0;

    // Send the data
    esp_err_t result = esp_now_send(broadcastAddress, (uint8_t *) &myData, sizeof(myData));

    if (result == ESP_OK) {
        Serial.println("Data sent!");
        Serial.printf("ID: %d\tData ID: %d\tData: %.2f\n", myData.id, myData.data_id, myData.data);
    } else {
        Serial.println("Error sending data");
    }

    // Wait before sending next data
    delay(1000);
}