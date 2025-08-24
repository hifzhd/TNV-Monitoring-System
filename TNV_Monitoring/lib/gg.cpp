#include <FS.h>
#include <LittleFS.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <U8g2lib.h>
#include <Preferences.h>
#include <SPI.h>
#include <Ethernet.h>

// ===== Pins Used =====
#define TEMP_PIN 13   // Temperature sensor
#define BUZZER_PIN 12
#define BTN_UP 14     // Button to increase offset
#define BTN_DOWN 27   // Button to decrease offset
#define BTN_LOG 26    // Button to send .txt file
#define W5500_SS 5
#define W5500_SCK 18
#define W5500_MISO 19
#define W5500_MOSI 23

// ===== Ethernet Setup =====
byte mac[] = {0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED}; // unique MAC
IPAddress IP(169, 254, 95, 177); // assigned ESP32 IP address
IPAddress serverIP(169, 254, 95, 118); // PC ip address
uint16_t serverPort = 5000;

EthernetClient client;

// ===== Calibration =====
float tempOffset = 0.0; // Initial calibration offset
Preferences prefs;      // To store offset even when device is not powered on

// ===== Debouncing & Timing =====
const unsigned long DEBOUNCE_MS = 500;
const unsigned long READ_INTERVAL = 1000; // in ms
unsigned long lastTempRead = 0;

// ===== Interrupt Flags =====
volatile bool upPressed = false;
volatile bool downPressed = false;
volatile bool logPressed = false;

// ===== Logging State =====
bool isLogging = false;
const char *LOG_FILENAME = "/temperature_log.txt";

// ===== Ethernet Functions =====
void initEthernet() {
  SPI.begin(W5500_SCK, W5500_MISO, W5500_MOSI, W5500_SS);
  Ethernet.init(W5500_SS);
  Ethernet.begin(mac, IP);

  delay(1000); // delay for ethernet to setup
  Serial.print("Ethernet IP: ");
  Serial.println(Ethernet.localIP());
}

void sendFileOverEthernet() {
  if (client.connect(serverIP, serverPort)) {
    Serial.println("Connected to server. Sending file...");

    File file = LittleFS.open(LOG_FILENAME, FILE_READ);
    if (!file) {
      Serial.println("Failed to open log file!");
      client.stop();
      return;
    }

    while (file.available()) {
      client.write(file.read());
    }

    file.close();
    client.stop();
    Serial.println("File sent and connection closed.");
  } else {
    Serial.println("Failed to connect to server.");
  }
}

// ===== OLED =====
static const unsigned char image_Layer_3_bits[] = {0xf8,0xff,0xff,0x01,0xfc,0xff,0xff,0x03,0xfe,0xff,0xff,0x07,0xff,0xff,0xff,0x0f,0xff,0xff,0xff,0x0f,0xff,0xff,0xff,0x0f,0xff,0xff,0xff,0x0f,0xff,0xff,0xff,0x0f,0xff,0xff,0xff,0x0f,0xff,0xff,0xff,0x0f,0xff,0xff,0xff,0x0f,0xff,0xff,0xff,0x0f,0xff,0xff,0xff,0x0f,0xff,0xff,0xff,0x0f,0xff,0xff,0xff,0x0f,0xff,0xff,0xff,0x0f,0xff,0xff,0xff,0x0f,0xff,0xff,0xff,0x0f,0xff,0xff,0xff,0x0f,0xff,0xff,0xff,0x0f,0xff,0xff,0xff,0x0f,0xff,0xff,0xff,0x0f,0xff,0xff,0xff,0x0f,0xff,0xff,0xff,0x0f,0xff,0xff,0xff,0x0f,0xfe,0xff,0xff,0x07,0xfc,0xff,0xff,0x03,0xf8,0xff,0xff,0x01};
static const unsigned char image_weather_temperature_bits[] = {0x38,0x00,0x44,0x40,0xd4,0xa0,0x54,0x40,0xd4,0x1c,0x54,0x06,0xd4,0x02,0x54,0x02,0x54,0x06,0x92,0x1c,0x39,0x01,0x75,0x01,0x7d,0x01,0x39,0x01,0x82,0x00,0x7c,0x00};

U8G2_SSD1306_128X32_UNIVISION_F_HW_I2C u8g2(U8G2_R0,U8X8_PIN_NONE);

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

void dispOLED(float tempC) {
  u8g2.setFont(u8g2_font_helvB08_tr);
  u8g2.clearBuffer();
  u8g2.setCursor(0, 15);
  u8g2.print("Temp: ");
  u8g2.print(tempC, 2);
  u8g2.print(" C");

  u8g2.setCursor(0, 30);
  u8g2.print("Offset: ");
  u8g2.print(tempOffset, 2);
  u8g2.print(" C");
  
  u8g2.sendBuffer();
}

// ===== DS18B20 =====
OneWire oneWire(TEMP_PIN);
DallasTemperature sensors(&oneWire);

void initTempSensor() {
  sensors.begin();
  Serial.println("Temp Sensor Initialized");
  sensors.requestTemperatures();
  delay(750);
  sensors.getTempCByIndex(0); // Discard first value
}

float readTempSensor() {
  sensors.requestTemperatures();
  float tempC = sensors.getTempCByIndex(0);
  if (tempC == 85.0 || tempC == DEVICE_DISCONNECTED_C) return NAN;
  return tempC + tempOffset;
}

// ===== Buzzer =====
void initBuzzer() {
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);
}

void tempCheck(float tempC) {
  if (!isnan(tempC) && (tempC < -25.0 || tempC > 45.0)) {
    digitalWrite(BUZZER_PIN, HIGH); // Buzzer ON if temp is outside of normal operating range
  } else {
    digitalWrite(BUZZER_PIN, LOW);
  }
}

// ===== Interrupts =====
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
    logPressed = true;
    lastISRTime = now;
  }
}

// ===== Buttons =====
void initButtons() {
  pinMode(BTN_UP, INPUT_PULLUP);
  pinMode(BTN_DOWN, INPUT_PULLUP);
  pinMode(BTN_LOG, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(BTN_UP), onUpPress, FALLING);
  attachInterrupt(digitalPinToInterrupt(BTN_DOWN), onDownPress, FALLING);
  attachInterrupt(digitalPinToInterrupt(BTN_LOG), onLogPress, FALLING);
}

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

  if (logPressed) {
    // Toggle logging state
    if (!isLogging) {
      // Start logging
      File file = LittleFS.open(LOG_FILENAME, FILE_WRITE);
      if (file) {
        file.close();
        isLogging = true;
        Serial.println("\n=== LOGGING STARTED ===");
        Serial.println("Logging will be recorded to LittleFS.");
      } else {
        Serial.println("Failed to create log file, logging not started");
      }
    } else {
      // Stop logging, then send and delete file
      isLogging = false;
      Serial.println("\n=== LOGGING STOPPED ===");
      // Send file over serial
      Serial.println("Sending file...");
      sendFileOverEthernet();
      // Delete file to avoid flash wear
      if (LittleFS.remove(LOG_FILENAME)) {
        Serial.println("Log file deleted fron LittleFS");
      } else {
        Serial.println("Failed to delete log file");
      }
    }
    logPressed = false;
  }
}

// ===== LittleFS Setup =====
void initLittleFS() {
  if (!LittleFS.begin(true)) {
    Serial.println("LittleFS mount failed");
  }
  else Serial.println("LittleFS mounted successfully");
}

void logTempToFile(float tempC) {
  File file = LittleFS.open(LOG_FILENAME, FILE_APPEND);
  if (!file) {
    Serial.println("Failed to open file for appending!");
    return;
  }
  // Add timestamp based on millis()
  unsigned long t = millis();
  file.printf("%lu  | Temperature: %.2f C  | Offset: %.2f C\n", t, tempC, tempOffset);
  file.close();
}

// void sendFileOverSerial() {
//   File file = LittleFS.open(LOG_FILENAME, FILE_READ);
//   if (!file) {
//     Serial.println("Failed to open log file for reading!");
//     return;
//   }

//   Serial.println("===START_OF_FILE===");
//   while (file.available()) {
//     Serial.write(file.read());
//   }
//   Serial.println("===END_OF_FILE===");

//   file.close();
// }

// ===== Saving Calibration Settings =====
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

// ===== Setup =====
void setup() {
  Serial.begin(115200);
  loadTempOffset(); // load offset first
  initTempSensor();
  initBuzzer();
  initButtons();
  initOLED(); // show splash screen
  delay(3000);
  initLittleFS();
  initEthernet();
  Serial.println("Ready. Press LOG button to start logging.");
}

// ===== Main =====
void loop() {
  handleButtons();

  unsigned long now = millis();

  if (now - lastTempRead > READ_INTERVAL) {
    float tempC = readTempSensor();

    if (!isnan(tempC)) {
      Serial.printf("Temperature: %.2f °C | Offset: %.2f\n", tempC, tempOffset);
      dispOLED(tempC);
      tempCheck(tempC);
      if (isLogging) {
        logTempToFile(tempC);  
      }
    } else {
      Serial.println("Invalid temperature reading");
    }

    lastTempRead = now;
  }
}