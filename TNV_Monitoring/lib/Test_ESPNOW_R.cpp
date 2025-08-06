#include <Arduino.h>
#include <esp_now.h>
#include <WiFi.h>

// MAC Adress
uint8_t broadcastAddress[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

// Structure to receive data
typedef struct struct_message {
  int id;
  u8_t data_id;
  float data;
} struct_message;

// Create a struct_message called myData
struct_message myData;

// Reveived Datas

struct_message board1;
struct_message board2;
struct_message board3;

// Array to hold received data
struct_message receivedData[3] = {board1, board2, board3};

// Callback when data is received
void OnDataRecv(const uint8_t *mac_addr, const uint8_t *incomingData, int len) {
    char macStr[18];
    Serial.printf("Received data from: ");
    snprintf(macStr, sizeof(macStr), "%02x:%02x:%02x:%02x:%02x:%02x",
        mac_addr[0], mac_addr[1], mac_addr[2],
        mac_addr[3], mac_addr[4], mac_addr[5]);
    Serial.println(macStr);
    memcpy(&myData, incomingData, sizeof(myData));
    Serial.printf("Board ID: %d\rData ID: %d\tData: %.2f\n", myData.id, myData.data_id, myData.data);

    // Store the received data in the struct
    receivedData[myData.id - 1] = myData;
}

void setup() {
    Serial.begin(115200);
    WiFi.mode(WIFI_STA);
    Serial.println("ESP-NOW Receiver started");

    // Initialize ESP-NOW
    if (esp_now_init() != ESP_OK) {
        Serial.println("Error initializing ESP-NOW");
        return;
    }
    
    // Register the receive callback
    esp_now_register_recv_cb(esp_now_recv_cb_t(OnDataRecv));
}

void loop(){
    Serial.printf("ID:%d %d %d Data 1: %d\t Data 2: %d\t Data 3: %d\n", 
                  receivedData[0].data_id, receivedData[1].data_id, receivedData[2].data_id,
                  receivedData[0].data, receivedData[1].data, receivedData[2].data);
    delay(1000);
}