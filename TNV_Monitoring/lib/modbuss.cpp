// INCLUDES AND DEFINES -------------------------------------------------------------------------------------
#include <Arduino.h>
#include <LittleFS.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <U8g2lib.h>
#include <Preferences.h>
#include <SPI.h>
#include <Ethernet.h>
#include <ModbusEthernet.h>

#define TEMP 13  
#define BUZZER 12
#define BUTTON_UP 14
#define BUTTON_DOWN 27
#define BUTTON_LOG 26
#define ETH_SCK 18
#define ETH_MISO 19
#define ETH_MOSI 23
#define ETH_CS 5
#define ETH_RST 4

// GLOBAL VARIABLES ----------------------------------------------------------------------------------------
// Ethernet setup
byte mac[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };
IPAddress espIP(192, 168, 1, 177);      // ESP32 IP address
IPAddress serverIP(192, 168, 1, 100);   // Modbus server IP 
ModbusEthernet mb;
uint16_t myData = 123;                  // Data to send
const int REG_ADDR = 512;               // Modbus register on server (PC)

unsigned long lastSend = 0;
const unsigned long SEND_INTERVAL = 2000; // send every 2 seconds

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

  mb.client(); // ESP32 is Modbus TCP client
}

void loop() {
  mb.task(); // Keep Modbus running

  // Connect if not connected
  if (!mb.isConnected(serverIP)) {
    mb.connect(serverIP);
  }

  // Send data periodically
  if (millis() - lastSend > SEND_INTERVAL) {
    lastSend = millis();

    if (mb.isConnected(serverIP)) {
      if (mb.writeHreg(serverIP, REG_ADDR, myData)) {
        Serial.print("Data sent: ");
        Serial.println(myData);
      } else {
        Serial.println("Failed to send data.");
      }
    } else {
      Serial.println("Connecting...");
    }

    // Update your data here for next send
    myData++; // example: incrementing data
  }
}
