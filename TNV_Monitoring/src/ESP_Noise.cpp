// LIBRARIES & PINS ----------------------------------------------------------------------------------------
#include <Arduino.h>
#include <esp_now.h>
#include <WiFi.h>
#include <Wire.h>
#include <U8g2lib.h>
#include <math.h>
#define LED 13
#define BUZZER 12
#define VIBSIG 34

// GLOBAL VARIABLES ----------------------------------------------------------------------------------------
U8G2_SSD1306_128X32_UNIVISION_F_HW_I2C u8g2(U8G2_R0, /* reset=*/ U8X8_PIN_NONE);
static const unsigned char image_microphone_recording_bits[] = {0x30,0xf0,0x03,0x03,0x30,0xf0,0x03,0x03,0xc0,0xfc,0xcf,0x00,0xc0,0xfc,0xcf,0x00,0x00,0xcc,0x0c,0x00,0x00,0xcc,0x0c,0x00,0x3c,0xfc,0x0f,0x0f,0x3c,0xfc,0x0f,0x0f,0x00,0xcc,0x0c,0x00,0x00,0xcc,0x0c,0x00,0xc0,0xfc,0xcf,0x00,0xc0,0xfc,0xcf,0x00,0x30,0xcc,0x0c,0x03,0x30,0xcc,0x0c,0x03,0x00,0xfc,0x0f,0x00,0x00,0xfc,0x0f,0x00,0x00,0xfc,0x0f,0x00,0x00,0xfc,0x0f,0x00,0xc0,0xf0,0xc3,0x00,0xc0,0xf0,0xc3,0x00,0x00,0x03,0x30,0x00,0x00,0x03,0x30,0x00,0x00,0xfc,0x0f,0x00,0x00,0xfc,0x0f,0x00,0x00,0xc0,0x00,0x00,0x00,0xc0,0x00,0x00,0x00,0xc0,0x00,0x00,0x00,0xc0,0x00,0x00,0x00,0xfc,0x0f,0x00,0x00,0xfc,0x0f,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00};
static const unsigned char image_ButtonRight_bits[] = {0x01,0x03,0x07,0x0f,0x07,0x03,0x01};
static const unsigned char image_network_4_bars_bits[] = {0x00,0x70,0x00,0x70,0x00,0x70,0x00,0x70,0x00,0x77,0x00,0x77,0x00,0x77,0x00,0x77,0x70,0x77,0x70,0x77,0x70,0x77,0x70,0x77,0x77,0x77,0x77,0x77,0x77,0x77,0x00,0x00};
static const unsigned char image_network_not_connected_bits[] = {0x41,0x70,0x22,0x50,0x14,0x50,0x08,0x50,0x14,0x57,0x22,0x55,0x41,0x55,0x00,0x55,0x70,0x55,0x50,0x55,0x50,0x55,0x50,0x55,0x57,0x55,0x55,0x55,0x77,0x77,0x00,0x00};

uint8_t broadcastAddress[] = {0xEC, 0xE3, 0x34, 0x21, 0x91, 0x74};                // Temp. device mac address
typedef struct struct_message {
  int id;
  u8_t data_id;
  float data;
} struct_message;
struct_message myData;    // Message to be sent via ESP-NOW
esp_now_peer_info_t peerInfo;

bool espnow = true;
float noiseLevel = 0.0;   // Measured noise level in dB
float ref_V =  0.02394;
float ref_dB = 94.0;
uint8_t mode = 0;          // Display and calibration mode (0 value, 1 graph, 2 calibration)

// FUNCTIONS -----------------------------------------------------------------------------------------------
void SendData();
void displayData();
void ReadData();
void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status);
void StartDisplay();

// SETUP & LOOP --------------------------------------------------------------------------------------------
void setup() {
  Serial.begin(115200);
  Serial.println("Vibration Monitoring Device");
  Serial.println("ESP-NOW Sender 1 started");

  analogReadResolution(12);
  u8g2.begin();

  pinMode(LED, OUTPUT);
  pinMode(BUZZER, OUTPUT);
  pinMode(VIBSIG, INPUT);

  // Initialize ESP-NOW
  WiFi.mode(WIFI_STA);
  if (esp_now_init() != ESP_OK) {
    Serial.println("Error initializing ESP-NOW");
    espnow = false;
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
    espnow = false;
    return;
  }

  // Starting OLED display
  StartDisplay();
  delay(3000);
}

void loop(){
    ReadData();
    //SendData();
    digitalWrite(LED, LOW);
    delay(10);
}

// FUNCTIONS -----------------------------------------------------------------------------------------------
void SendData() {
    myData.id = 1; 
    myData.data_id = random(0, 255); 
    myData.data = random(0, 100) / 10.0;
    digitalWrite(LED, HIGH);
  
    esp_err_t result = esp_now_send(broadcastAddress, (uint8_t *) &myData, sizeof(myData));

    if (result == ESP_OK) {
        Serial.println("Data sent!");
        Serial.printf("ID: %d\tData ID: %d\tData: %.2f\n", myData.id, myData.data_id, myData.data);
    } else {
        Serial.println("Error sending data");
    }
    delay(10);
}

void displayData() {
    // OLED display
    char buffer[20];
    snprintf(buffer, sizeof(buffer), "%.2fdB", noiseLevel);
    u8g2.clearBuffer();
    u8g2.setFontMode(1);
    u8g2.setBitmapMode(1);
    if (espnow){
      u8g2.drawXBM(111, 1, 15, 16, image_network_4_bars_bits);
    }
    else if (!espnow){
      u8g2.drawXBM(111, 1, 15, 16, image_network_not_connected_bits);
    }
    u8g2.drawXBM(121, 22, 4, 7, image_ButtonRight_bits);
    u8g2.setFont(u8g2_font_t0_22b_tr);
    u8g2.drawStr(0, 29, buffer);
    u8g2.setFont(u8g2_font_helvB08_tr);
    u8g2.drawStr(3, 11, "Noise Level");
    u8g2.sendBuffer();
}

void ReadData() {
  const int samples = 400; // Number of samples per batch
  uint32_t sum = 0;
  uint64_t sumSq = 0;
  
  for (int i = 0; i < samples; i++) {
    int adcValue = analogRead(VIBSIG);
    sum += adcValue;
    sumSq += (uint64_t)adcValue * adcValue;
    delayMicroseconds(50); // 20 kHz sampling rate
  }
  
  float mean = (float)sum / samples;
  float meanSq = (float)sumSq / samples;
  float rmsADC = sqrtf(meanSq - (mean * mean));
  float rmsVoltage = rmsADC * (3.3f / 4096.0f);
  // Compute dB only if rmsVoltage > 0
  noiseLevel = (rmsVoltage > 0) ? ref_dB + 20.0f * log10f(rmsVoltage / ref_V) : 0;
}


void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
  Serial.print("Send Status: ");
  Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Success" : "Fail");
}

void StartDisplay() {
  u8g2.clearBuffer();
  u8g2.setFontMode(1);
  u8g2.setBitmapMode(1);
  u8g2.drawXBM(3, 1, 30, 32, image_microphone_recording_bits);
  u8g2.setFont(u8g2_font_helvB08_tr);
  u8g2.drawStr(43, 11, "DECIBEL METER");
  u8g2.setFont(u8g2_font_5x8_tr);
  u8g2.drawStr(45, 21, "T-N-V Monitoring");
  u8g2.drawStr(94, 30, "System");
  u8g2.setDrawColor(2);
  u8g2.drawRBox(3, 0, 31, 32, 5);
  u8g2.sendBuffer();
}