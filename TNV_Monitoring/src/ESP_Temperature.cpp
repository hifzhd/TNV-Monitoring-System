// INCLUDES AND DEFINES -------------------------------------------------------------------------------------
#include <Arduino.h>
#include <FS.h>
#include <LittleFS.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <U8g2lib.h>
#include <Preferences.h>
#include <SPI.h>
#include <Ethernet.h>
#include <ModbusEthernet.h>
#include <esp_now.h>
#include <WiFi.h>

#define BOARD_ID 3
#define TEMP 13  
#define BUZZER 12
#define LED 25
#define BUTTON_UP 14
#define BUTTON_DOWN 27
#define BUTTON_LOG 26
#define ETH_SCK 18
#define ETH_MISO 19   123.67
#define ETH_MOSI 23
#define ETH_CS 5
#define ETH_RST 4
#define ESP_NOW_SEND 5

// GLOBAL VARIABLES ----------------------------------------------------------------------------------------
static const unsigned char image_Layer_3_bits[] = {0xf8,0xff,0xff,0x01,0xfc,0xff,0xff,0x03,0xfe,0xff,0xff,0x07,0xff,0xff,0xff,0x0f,0xff,0xff,0xff,0x0f,0xff,0xff,0xff,0x0f,0xff,0xff,0xff,0x0f,0xff,0xff,0xff,0x0f,0xff,0xff,0xff,0x0f,0xff,0xff,0xff,0x0f,0xff,0xff,0xff,0x0f,0xff,0xff,0xff,0x0f,0xff,0xff,0xff,0x0f,0xff,0xff,0xff,0x0f,0xff,0xff,0xff,0x0f,0xff,0xff,0xff,0x0f,0xff,0xff,0xff,0x0f,0xff,0xff,0xff,0x0f,0xff,0xff,0xff,0x0f,0xff,0xff,0xff,0x0f,0xff,0xff,0xff,0x0f,0xff,0xff,0xff,0x0f,0xff,0xff,0xff,0x0f,0xff,0xff,0xff,0x0f,0xff,0xff,0xff,0x0f,0xfe,0xff,0xff,0x07,0xfc,0xff,0xff,0x03,0xf8,0xff,0xff,0x01};
static const unsigned char image_weather_temperature_bits[] = {0x38,0x00,0x44,0x40,0xd4,0xa0,0x54,0x40,0xd4,0x1c,0x54,0x06,0xd4,0x02,0x54,0x02,0x54,0x06,0x92,0x1c,0x39,0x01,0x75,0x01,0x7d,0x01,0x39,0x01,0x82,0x00,0x7c,0x00};
static const unsigned char image_ButtonRight_bits[] U8X8_PROGMEM = {0x01,0x03,0x07,0x0f,0x07,0x03,0x01};
static const unsigned char image_music_radio_broadcast_bits[] U8X8_PROGMEM = {0xe0,0x03,0x18,0x0c,0xe4,0x13,0x12,0x24,0xc9,0x49,0x25,0x52,0x95,0x54,0xc5,0x51,0x60,0x03,0xc0,0x01,0x80,0x00,0xc0,0x01,0x40,0x01,0x60,0x03,0x20,0x02,0x00,0x00};
U8G2_SSD1306_128X32_UNIVISION_F_HW_I2C u8g2(U8G2_R0,U8X8_PIN_NONE);

byte mac[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };
IPAddress espIP(192, 168, 1, 177);
IPAddress serverIP(192, 168, 1, 100);
ModbusEthernet mb;

OneWire oneWire(TEMP);
DallasTemperature sensors(&oneWire);

uint16_t dataTemperature[3];  // (A.a) Temperature in Celsius
uint16_t dataSPL[2];          // (A.b) Sound Pressure Level in dB
uint16_t dataVibration[5];    // (A-B-C-D-E) 5 different dominaant frequencies in Hz

const int regAddrTem = 510;
const int regAddrSPL = regAddrTem+3;
const int regAddrVib = regAddrSPL+2;
unsigned long lastSend = 0;
const unsigned long SEND_INTERVAL = 5000;

uint8_t broadcastAddress[] = {0xEC, 0xE3, 0x34, 0x21, 0x91, 0x74};                // Temp. device mac address
typedef struct struct_message {
  int id;
  float data;
  uint16_t datafreq[5];
} struct_message;
struct_message myData; // Struct to store data sent by other ESP32
struct_message receivedData[2];

float tempOffset = 0.0;
Preferences prefs;

float tempLevel = 0;
const unsigned long DEBOUNCE_MS = 500;
const unsigned long READ_INTERVAL = 1000; // in ms
unsigned long lastTempRead = 0;
volatile bool upPressed = false;
volatile bool downPressed = false;
volatile bool logging = false;
volatile bool flagSend = false;
hw_timer_t *timer = NULL;
volatile bool receivingESPNOW = false;

// FUNCTIONS -----------------------------------------------------------------------------------------------
void initOLED();
void displayData1();
void initTempSensor();
void handleButtons();
void IRAM_ATTR onUpPress();
void IRAM_ATTR onDownPress();
void IRAM_ATTR onLogPress();
void loadTempOffset();
void saveTempOffset();
void IRAM_ATTR onTimer();
float readTemperature();
void updateData(uint8_t id);
void OnDataRecv(const uint8_t *mac_addr, const uint8_t *incomingData, int len);

void setup() {
  Serial.begin(115200);

  // Reset W5500
  pinMode(ETH_RST, OUTPUT);
  digitalWrite(ETH_RST, LOW);
  delay(200);
  digitalWrite(ETH_RST, HIGH);
  delay(200);

  // Init Ethernet
  Ethernet.init(ETH_CS);
  Ethernet.begin(mac, espIP);
  Serial.print("ESP32 IP: ");
  Serial.println(Ethernet.localIP());

  pinMode(BUZZER, OUTPUT);
  pinMode(LED, OUTPUT);
  digitalWrite(LED, LOW);
  pinMode(BUTTON_UP, INPUT_PULLUP);
  pinMode(BUTTON_DOWN, INPUT_PULLUP);
  pinMode(BUTTON_LOG, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(BUTTON_UP), onUpPress, FALLING);
  attachInterrupt(digitalPinToInterrupt(BUTTON_DOWN), onDownPress, FALLING);
  attachInterrupt(digitalPinToInterrupt(BUTTON_LOG), onLogPress, FALLING);

  loadTempOffset();
  initTempSensor();
  mb.client(); // ESP32 is Modbus TCP client
  initOLED();
  delay(3000);

  WiFi.mode(WIFI_STA);
  if (esp_now_init() != ESP_OK) {
    Serial.println("Error initializing ESP-NOW");
    return;
  }
  esp_now_register_recv_cb(OnDataRecv);

  timer = timerBegin(0, 80, true);
  timerAttachInterrupt(timer, &onTimer, true);
  timerAlarmWrite(timer, ESP_NOW_SEND * 1000000, true);
  timerAlarmEnable(timer);
}

void loop() {
  tempLevel = readTemperature();
  updateData(3);
  displayData1();
  handleButtons();
  digitalWrite(LED, LOW);
  delay(500);
}

// FUNCTIONS -----------------------------------------------------------------------------------------------
// Data reading and processing - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 
void initTempSensor() {
  sensors.begin();
  sensors.setResolution(12);
  Serial.println("Temperature sensor initialized.");
  sensors.requestTemperatures();
  delay(750);
  sensors.getTempCByIndex(0);
}

float readTemperature() {
  sensors.requestTemperatures();
  float tempC = sensors.getTempCByIndex(0);
  if (tempC == 85.0 || tempC == DEVICE_DISCONNECTED_C) return NAN;
  return tempC + tempOffset;
}

void updateData(uint8_t id){
  // Noise Level
  dataSPL[0] = (uint16_t) receivedData[0].data;
  float decimals = (receivedData[0].data - (int)receivedData[0].data) * 100.0f;
  dataTemperature[2] = (uint16_t)(decimals + 0.5f);  // rounding

  // Frequencies
  for (int i=0; i < 5; i++){
    dataVibration[i] = receivedData[1].datafreq[i];
  }

  // Temperature
  if (tempLevel < 0) {
    dataTemperature[0] = 1;   // negative
    tempLevel = -tempLevel;   // make it positive for extraction
  } else {
    dataTemperature[0] = 0;   // positive
  }
  dataTemperature[1] = (uint16_t)tempLevel;
  float decimals = (tempLevel - (int)tempLevel) * 10000.0f;
  dataTemperature[2] = (uint16_t)(decimals + 0.5f);  // rounding
}

// ESP-NOW - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 
void OnDataRecv(const uint8_t *mac_addr, const uint8_t *incomingData, int len) {
  timerAlarmDisable(timer);
  memcpy(&myData, incomingData, sizeof(myData));
  if (myData.id >= 1 && myData.id <= 2) {
      receivedData[myData.id - 1] = myData;
  }
  Serial.printf("Received Data from board id: %d", myData.id);
  timerAlarmEnable(timer);
}

// Display & Menu - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 
void initOLED() {
  u8g2.begin();
  u8g2.clearBuffer();
  u8g2.setFontMode(1);
  u8g2.setBitmapMode(1);
  u8g2.drawXBM(4, 2, 28, 28, image_Layer_3_bits);
  u8g2.setDrawColor(2);
  u8g2.drawXBM(10, 8, 16, 16, image_weather_temperature_bits);
  u8g2.setDrawColor(1);
  u8g2.setFont(u8g2_font_helvB08_tr);
  u8g2.drawStr(40, 10, "TEMP MONITOR");
  u8g2.setFont(u8g2_font_5x8_tr);
  u8g2.drawStr(49, 21, "T-N-V Measuring");
  u8g2.drawStr(93, 29, "System");
  u8g2.sendBuffer();
}

void displayData1(){
  char buffer[20];
  snprintf(buffer, sizeof(buffer), "%.2f C", tempLevel);
  u8g2.clearBuffer();
  u8g2.setFontMode(1);
  u8g2.setBitmapMode(1);
  u8g2.setFont(u8g2_font_t0_22b_tr);
  u8g2.drawStr(0, 29, buffer);
  u8g2.setFont(u8g2_font_helvB08_tr);
  u8g2.drawStr(3, 11, "Temperature");
  u8g2.setFont(u8g2_font_4x6_tr);
  snprintf(buffer, sizeof(buffer), "Offset: %.2f", tempOffset);
  u8g2.drawStr(76, 10, buffer);
  u8g2.drawXBMP(116, 22, 4, 7, image_ButtonRight_bits);
  u8g2.sendBuffer();
}

// Button and Interrupts - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 
void IRAM_ATTR onUpPress() {
  static unsigned long lastISRTime = 0;
  unsigned long now = millis();
  if (now - lastISRTime > DEBOUNCE_MS) { // 200 ms debounce
    upPressed = true;
    lastISRTime = now;
  }
}

void IRAM_ATTR onDownPress() {
  static unsigned long lastISRTime = 0;
  unsigned long now = millis();
  if (now - lastISRTime > DEBOUNCE_MS) { // 200 ms debounce
    downPressed = true;
    lastISRTime = now;
  }
}

void IRAM_ATTR onLogPress() {
  static unsigned long lastISRTime = 0;
  unsigned long now = millis();
  if (now - lastISRTime > DEBOUNCE_MS) { // 200 ms debounce
    logging = !logging;
    lastISRTime = now;
  }
}

void IRAM_ATTR onTimer() {
  flagSend = true; // Set flag to indicate data should be sent
}

// Buttons - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 
void handleButtons() {
  if (upPressed) {
    tempOffset += 0.05;
    if (fabs(tempOffset) < 0.001) tempOffset = 0.0;
    Serial.printf("Offset increased: %.2f\n", tempOffset);
    saveTempOffset(); // save offset immediately
    upPressed = false;
  }

  if (downPressed) {
    tempOffset -= 0.05;
    if (fabs(tempOffset) < 0.001) tempOffset = 0.0;
    Serial.printf("Offset decreased: %.2f\n", tempOffset);
    saveTempOffset(); // save offset immediately
    downPressed = false;
  }

  if (logging) {
    mb.task();
    if (mb.isConnected(serverIP)){
      mb.connect(serverIP);
    }
    if (flagSend){
      digitalWrite(LED, HIGH);
      delay(1);
      flagSend = false;
      if (mb.isConnected(serverIP)){
        mb.writeHreg(serverIP, regAddrTem, dataTemperature[0]);
        mb.writeHreg(serverIP, regAddrTem+1, dataTemperature[1]);
        mb.writeHreg(serverIP, regAddrTem+2, dataTemperature[2]);
        mb.writeHreg(serverIP, regAddrSPL, dataSPL[0]);
        mb.writeHreg(serverIP, regAddrSPL+1, dataSPL[1]);
        for (int i = 0; i < 5; i++) {
          mb.writeHreg(serverIP, regAddrVib + i, dataVibration[i]);
        }
      }
      else{
        Serial.println("Connecting to Modbus server...");
        mb.connect(serverIP);
      }
    }
  }
}

// Loading Data - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 
void loadTempOffset() {
  prefs.begin("settings", true); // true = read only
  tempOffset = prefs.getFloat("offset", 0.0); // tempOffset = 0.0 if not found
  prefs.end();
  Serial.printf("Loaded offset from NVS: %.2f\n", tempOffset);
}

void saveTempOffset() {
  prefs.begin("settings", false); // read/write
  prefs.putFloat("offset", tempOffset);
  prefs.end();
  Serial.printf("Saved offset to NVS: %.2f\n", tempOffset);
}