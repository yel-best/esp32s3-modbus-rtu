# ESP32-S3 Modbus RTU Project

## Overview
ESP32-S3 support for Modbus RTU, WiFi, and MQTT using ESP-IDF v5.4.4

## Features
- **Modbus RTU**: Serial communication with Modbus RTU protocol
- **WiFi**: Wi-Fi connectivity (STA/SoftAP modes)
- **MQTT**: MQTT broker client/server support
- **ESP-IDF v5.4.4**: Latest ESP-IDF framework

## Project Structure
```
esp32s3-modbus-rtu/
├── src/              # Application source code
├── include/          # Header files
├── docs/             # Documentation
└── README.md         # This file
```

## Getting Started
1. Ensure ESP-IDF v5.4.4 is installed and configured
2. Build: `idf.py build`
3. Flash: `idf.py -p /dev/ttyUSB0 flash monitor`

---
Created by Qwythos (Empero AI)
