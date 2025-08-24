// INCLUDES AND DEFINES -------------------------------------------------------------------------------------
#include <Arduino.h>
#include <esp_now.h>
#include <WiFi.h>
#include <Wire.h>
#include <U8g2lib.h>
#include <Preferences.h>

#define GRAPH_WIDTH 128   
#define GRAPH_HEIGHT 32   
#define MAX_VALUE 4095    
#define LED 13
#define BUZZER 12
#define VIBSIG 34
#define BUTTON_MODE 26
#define BUTTON_UP 25
#define BUTTON_DOWN 33
#define ESP_NOW_SEND 5

// GLOBAL VARIABLES ----------------------------------------------------------------------------------------
U8G2_SSD1306_128X32_UNIVISION_F_HW_I2C u8g2(U8G2_R0, /* reset=*/ U8X8_PIN_NONE);
static const unsigned char image_microphone_recording_bits[] U8X8_PROGMEM = {0x30,0xf0,0x03,0x03,0x30,0xf0,0x03,0x03,0xc0,0xfc,0xcf,0x00,0xc0,0xfc,0xcf,0x00,0x00,0xcc,0x0c,0x00,0x00,0xcc,0x0c,0x00,0x3c,0xfc,0x0f,0x0f,0x3c,0xfc,0x0f,0x0f,0x00,0xcc,0x0c,0x00,0x00,0xcc,0x0c,0x00,0xc0,0xfc,0xcf,0x00,0xc0,0xfc,0xcf,0x00,0x30,0xcc,0x0c,0x03,0x30,0xcc,0x0c,0x03,0x00,0xfc,0x0f,0x00,0x00,0xfc,0x0f,0x00,0x00,0xfc,0x0f,0x00,0x00,0xfc,0x0f,0x00,0xc0,0xf0,0xc3,0x00,0xc0,0xf0,0xc3,0x00,0x00,0x03,0x30,0x00,0x00,0x03,0x30,0x00,0x00,0xfc,0x0f,0x00,0x00,0xfc,0x0f,0x00,0x00,0xc0,0x00,0x00,0x00,0xc0,0x00,0x00,0x00,0xc0,0x00,0x00,0x00,0xc0,0x00,0x00,0x00,0xfc,0x0f,0x00,0x00,0xfc,0x0f,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00};
static const unsigned char image_ButtonRight_bits[] U8X8_PROGMEM = {0x01,0x03,0x07,0x0f,0x07,0x03,0x01};
static const unsigned char image_music_radio_broadcast_bits[] U8X8_PROGMEM = {0xe0,0x03,0x18,0x0c,0xe4,0x13,0x12,0x24,0xc9,0x49,0x25,0x52,0x95,0x54,0xc5,0x51,0x60,0x03,0xc0,0x01,0x80,0x00,0xc0,0x01,0x40,0x01,0x60,0x03,0x20,0x02,0x00,0x00};
static const unsigned char image_SmallArrowDown_bits[] U8X8_PROGMEM = {0x7f,0x3e,0x1c,0x08};
static const unsigned char image_SmallArrowUp_bits[] U8X8_PROGMEM = {0x08,0x1c,0x3e,0x7f};

Preferences prefs;

uint8_t broadcastAddress[] = {0xEC, 0xE3, 0x34, 0x21, 0x91, 0x74};
typedef struct struct_message {
  int id;
  u8_t data_id;
  float data;
} struct_message;
struct_message myData;    // Message to be sent via ESP-NOW
esp_now_peer_info_t peerInfo;

hw_timer_t *timer = NULL;
volatile int interruptCounter = 0;
bool flagSend = false; // Flag to indicate if data should be sent

// FUNCTIONS -----------------------------------------------------------------------------------------------
void SendData();
void ReadData();
void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status);
void onTimer();

// SETUP & LOOP --------------------------------------------------------------------------------------------
void setup() {
  Serial.begin(115200);
  Serial.println("Vibration Monitoring Device");
  Serial.println("ESP-NOW Device 3 started");

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
    return;
  }
  esp_now_register_send_cb(OnDataSent);

  memcpy(peerInfo.peer_addr, broadcastAddress, 6);
  peerInfo.channel = 0;
  peerInfo.encrypt = false;
  if (esp_now_add_peer(&peerInfo) != ESP_OK) {
    Serial.println("Failed to add peer");
    return;
  }

  delay(3000);
  timer = timerBegin(0, 80, true);
  timerAttachInterrupt(timer, &onTimer, true);
  timerAlarmWrite(timer, ESP_NOW_SEND * 1000000, true);
  timerAlarmEnable(timer);
}

void loop(){
    ReadData();
}

// FUNCTIONS -----------------------------------------------------------------------------------------------
// Data Aquisition and Processing - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 
void ReadData() {
    Serial.println(analogRead(VIBSIG));
    delay(10);
}

// ESP-NOW - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void SendData() {
    myData.id = 3; 
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







void IRAM_ATTR onTimer() {
  flagSend = true; // Set flag to indicate data should be sent
}