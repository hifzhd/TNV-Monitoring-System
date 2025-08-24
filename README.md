# TNV-Monitoring-System
In this project, we develop an ESP32 based instrumentation system that measures temperature, noise level (SPL), and vibration. Three of these sensor are wired seperately with their corresponding ESP32, but all of the data will be collected wirelessly to a single device via ESP-NOW comminication protocol. Then, the centralized data will be sent to a Modbus TCP/IP Server/Slave device. This architecture allows the measuring point to be seperated for different variables, while also easing the communication between the devices to the Modbus TCP/IP Server/Slave device

## Used Tools and Materials
### Hardware
- ESP32 DevKitC WROOM-32D V4 CH9102X
- DS18B20 Digital Temperature Sensor Module [SZH-SSBH-052]
- Electret Microphone Amplifier - MAX4466 with Adjustable Gain [ada-1063]
- Piezo Vibration Sensor Module Flexible Piezo Film Vibration Sensor
- W5500 Ethernet LAN TCP/IP Network Module
- Breadboard, cables, resistor, LED, and other passive and power components
### Software
In this project, the porgram is developed in Visual Studio Codes with platform.io extension. The program is then debugged using QtSerial Monitor. For testing the Modbus TCP/IP communication, the slave device is a laptop with a Modbus slave application to simulate a Modbus Server/Slave device.
- [Platform io](https://platformio.org/install)
- [Qt Serial Monitor](https://github.com/mich-w/QtSerialMonitor/)
- [Modbus Slave](https://www.modbustools.com/download.html)
