// INCLUDES AND DEFINES -------------------------------------------------------------------------------------
#include <Arduino.h>
#include <esp_now.h>
#include <WiFi.h>
#include <Wire.h>
#include <U8g2lib.h>
#include <math.h>
#include <EEPROM.h>

#define EEPROM_SIZE 16
#define REF_V_ADDRESS 0
#define REF_DB_ADDRESS 4
#define LED 13
#define BUZZER 12
#define VIBSIG 34
#define BUTTON_MODE 26
#define BUTTON_UP 25
#define BUTTON_DOWN 33

// GLOBAL VARIABLES ----------------------------------------------------------------------------------------
U8G2_SSD1306_128X32_UNIVISION_F_HW_I2C u8g2(U8G2_R0, /* reset=*/ U8X8_PIN_NONE);
static const unsigned char image_microphone_recording_bits[] U8X8_PROGMEM = {0x30,0xf0,0x03,0x03,0x30,0xf0,0x03,0x03,0xc0,0xfc,0xcf,0x00,0xc0,0xfc,0xcf,0x00,0x00,0xcc,0x0c,0x00,0x00,0xcc,0x0c,0x00,0x3c,0xfc,0x0f,0x0f,0x3c,0xfc,0x0f,0x0f,0x00,0xcc,0x0c,0x00,0x00,0xcc,0x0c,0x00,0xc0,0xfc,0xcf,0x00,0xc0,0xfc,0xcf,0x00,0x30,0xcc,0x0c,0x03,0x30,0xcc,0x0c,0x03,0x00,0xfc,0x0f,0x00,0x00,0xfc,0x0f,0x00,0x00,0xfc,0x0f,0x00,0x00,0xfc,0x0f,0x00,0xc0,0xf0,0xc3,0x00,0xc0,0xf0,0xc3,0x00,0x00,0x03,0x30,0x00,0x00,0x03,0x30,0x00,0x00,0xfc,0x0f,0x00,0x00,0xfc,0x0f,0x00,0x00,0xc0,0x00,0x00,0x00,0xc0,0x00,0x00,0x00,0xc0,0x00,0x00,0x00,0xc0,0x00,0x00,0x00,0xfc,0x0f,0x00,0x00,0xfc,0x0f,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00};
static const unsigned char image_ButtonRight_bits[] U8X8_PROGMEM = {0x01,0x03,0x07,0x0f,0x07,0x03,0x01};
static const unsigned char image_music_radio_broadcast_bits[] U8X8_PROGMEM = {0xe0,0x03,0x18,0x0c,0xe4,0x13,0x12,0x24,0xc9,0x49,0x25,0x52,0x95,0x54,0xc5,0x51,0x60,0x03,0xc0,0x01,0x80,0x00,0xc0,0x01,0x40,0x01,0x60,0x03,0x20,0x02,0x00,0x00};
static const unsigned char image_SmallArrowDown_bits[] U8X8_PROGMEM = {0x7f,0x3e,0x1c,0x08};
static const unsigned char image_SmallArrowUp_bits[] U8X8_PROGMEM = {0x08,0x1c,0x3e,0x7f};


uint8_t broadcastAddress[] = {0xEC, 0xE3, 0x34, 0x21, 0x91, 0x74};                // Temp. device mac address
typedef struct struct_message {
  int id;
  u8_t data_id;
  float data;
} struct_message;
struct_message myData;    // Message to be sent via ESP-NOW
esp_now_peer_info_t peerInfo;

bool calibrated = false;
bool espnow = true;
float noiseLevel = 0.0;   // Measured noise level in dB
float ref_V =  0.02394;
float ref_dB = 94.0;
bool modeButton = false;      // Button state
uint8_t mode = 0;         // Display and calibration mode (0 value, 1 graph, 2 calibration)

// FUNCTIONS -----------------------------------------------------------------------------------------------
void SendData();
void DisplayData1();
void DisplayData2();
void DisplayCalibration();
void MenuDisplay();
float ReadData();
void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status);
void DisplayStart();
void EEPROMRead();
void EEPROMWrite();
void ButtonCheck();
void Calibrating();

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
  pinMode(BUTTON_MODE, INPUT_PULLUP);
  pinMode(BUTTON_UP, INPUT_PULLUP);
  pinMode(BUTTON_DOWN, INPUT_PULLUP);

  // Initialize ESP-NOW
  WiFi.mode(WIFI_STA);
  if (esp_now_init() != ESP_OK) {
    Serial.println("Error initializing ESP-NOW");
    espnow = false;
    return;
  }
  esp_now_register_send_cb(OnDataSent);
  memcpy(peerInfo.peer_addr, broadcastAddress, 6);
  peerInfo.channel = 0; // Use current channel
  peerInfo.encrypt = false;
  if (esp_now_add_peer(&peerInfo) != ESP_OK) {
    Serial.println("Failed to add peer");
    espnow = false;
    return;
  }

  // Loading Variables
  if (!EEPROM.begin(EEPROM_SIZE)) {
    Serial.println("Failed to initialise EEPROM");
    while (1);
  }
  EEPROMRead();

  // Starting OLED display
  DisplayStart();
  delay(3000);
}

void loop(){
    MenuDisplay();
}

// FUNCTIONS -----------------------------------------------------------------------------------------------
// Data Aquisition and Processing - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 
float ReadData() {
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
  float rmsVoltage = rmsADC * (3.3f / 4096.0);
  // Compute dB only if rmsVoltage > 0
  noiseLevel = (rmsVoltage > 0) ? ref_dB + 20.0f * log10f(rmsVoltage / ref_V) : 0;
  return noiseLevel;
}

// EEPROM Read and Write - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void EEPROMRead() {
  EEPROM.get(REF_V_ADDRESS, ref_V);
  EEPROM.get(REF_DB_ADDRESS, ref_dB);
  if (ref_V < 0.0 || ref_V > 3.3) {
    ref_V = 0.02394;
  }
  if (ref_dB < 0.0 || ref_dB > 120.0) {
    ref_dB = 94.0;
  }
  Serial.printf("EEPROM Read - REF_V: %.5f, REF_DB: %.2f\n", ref_V, ref_dB);
}

void EEPROMWrite() {
  EEPROM.put(REF_V_ADDRESS, ref_V);
  EEPROM.put(REF_DB_ADDRESS, ref_dB);
  if (EEPROM.commit()) {
    Serial.println("EEPROM Write Successful");
  } else {
    Serial.println("EEPROM Write Failed");
  }
}

// ESP-NOW - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
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

void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
  Serial.print("Send Status: ");
  Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Success" : "Fail");
}

// Display & Menu - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void MenuDisplay() {
  ButtonCheck();
  if (modeButton) {
    modeButton = false;
    mode++;
    if (mode > 2) {
      EEPROMWrite();
      mode = 0;
    }
  }
  
  switch (mode) {
    case 0:
      DisplayData1();
      break;
    case 1:
      DisplayData1(); // Placeholder for second data display
      //DisplayData2();
      break;
    case 2:
      //DisplayData1(); // Placeholder for calibration display
      DisplayCalibration();// Calibration display logic can be added here
      break;
    default:
      DisplayData1();
      break;
  }
}

void ButtonCheck() {
  if ((digitalRead(BUTTON_MODE) == LOW) && (modeButton % 3)!= 2) {
    modeButton = true;
    delay(200);
  }
  if (mode == 2){
    if (calibrated){
      if (digitalRead(BUTTON_UP) == LOW) {
        ref_dB += 0.5; 
        delay(200);
      }
      else if (digitalRead(BUTTON_DOWN) == LOW) {
        if (ref_dB > 0.0) {
          ref_dB -= 0.5; 
          delay(200);
        }
      }
      if (modeButton) {
        modeButton = true;
        calibrated = false;
        delay(200);
      }
    }
    else{
      if (!calibrated){
        if (digitalRead(BUTTON_UP) == LOW) {
          delay(200);
          Calibrating();
          digitalWrite(BUZZER, HIGH);
          delay(10);
          digitalWrite(BUZZER, LOW);
        }
        else if (digitalRead(modeButton) == LOW) {
          modeButton = true;
          delay(200);
        }
      }
    }
  }
}

void DisplayStart() {
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

void DisplayData1() {
  ReadData();
  char buffer[20];
  snprintf(buffer, sizeof(buffer), "%.2fdB", noiseLevel);
  u8g2.clearBuffer();
  u8g2.setFontMode(1);
  u8g2.setBitmapMode(1);
  u8g2.setFont(u8g2_font_t0_22b_tr);
  u8g2.drawStr(0, 29, buffer);
  u8g2.setFont(u8g2_font_helvB08_tr);
  u8g2.drawStr(3, 11, "Sound Intensity");
  u8g2.setDrawColor(2);
  u8g2.drawBox(109, 0, 19, 32);
  u8g2.drawXBMP(116, 22, 4, 7, image_ButtonRight_bits);
  if (espnow){
    u8g2.drawXBMP(111, 2, 15, 16, image_music_radio_broadcast_bits);
  }
  u8g2.sendBuffer();
}

void DisplayData2() {
    
}

void DisplayCalibration() {
  char buffer[20];
  snprintf(buffer, sizeof(buffer), "%.5fV", ref_V);
  u8g2.clearBuffer();
  u8g2.setFontMode(1);
  u8g2.setBitmapMode(1);
  u8g2.setFont(u8g2_font_helvB08_tr);
  u8g2.drawStr(3, 27, "V RMS :");
  u8g2.drawStr(3, 13, "Ref. dB :");
  u8g2.drawStr(47, 27, buffer);
  snprintf(buffer, sizeof(buffer), "%.2fdB", ref_dB);
  u8g2.drawStr(47, 13, buffer);

  if (!calibrated){
    u8g2.setFont(u8g2_font_5x8_tr);
    u8g2.drawStr(106, 10, "RE");
    u8g2.drawStr(107, 28, "OK");
  }
  else if (calibrated) {
    u8g2.setFont(u8g2_font_5x8_tr);
    u8g2.drawStr(107, 28, "OK");
  }
  
  u8g2.drawXBMP(118, 4, 7, 4, image_SmallArrowUp_bits);
  u8g2.drawXBMP(118, 13, 7, 4, image_SmallArrowDown_bits);
  u8g2.drawXBMP(119, 21, 4, 7, image_ButtonRight_bits);
  u8g2.setDrawColor(2);
  u8g2.drawBox(104, 0, 24, 32);
  u8g2.setDrawColor(1);
  u8g2.sendBuffer();
}

void Calibrating() {
  // Display calibration process
  u8g2.clearBuffer();
  u8g2.setFontMode(1);
  u8g2.setBitmapMode(1);
  u8g2.drawBox(-1, 0, 129, 32);
  u8g2.setDrawColor(2);
  u8g2.setFont(u8g2_font_helvB08_tr);
  u8g2.drawStr(25, 20, "CALIBRATING..");
  u8g2.sendBuffer();

  // VRMS calculation
  const int samples = 400; // Number of samples per batch
  uint32_t sum = 0;
  uint64_t sumSq = 0;
  for (int i = 0; i < samples; i++) {
    int adcValue = analogRead(VIBSIG);
    sum += adcValue;
    sumSq += (uint64_t)adcValue * adcValue;
    delayMicroseconds(50);
  }
  float mean = (float)sum / samples;
  float meanSq = (float)sumSq / samples;
  float rmsADC = sqrtf(meanSq - (mean * mean));
  float rmsVoltage = rmsADC * (3.3f / 4096.0f);
  ref_V = rmsVoltage;

  calibrated = true;
  delay(1000);
}