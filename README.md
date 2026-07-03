# ESP32-S3 Modbus RTU Project

## Overview
ESP32-S3 support for Modbus RTU, WiFi, and MQTT using ESP-IDF v5.4.4

## Features
- **Modbus RTU**: Serial communication with Modbus RTU protocol (9600bps)
- **WiFi**: Wi-Fi connectivity (STA/SoftAP modes)
- **MQTT**: MQTT broker client support
- **ESP-IDF v5.4.4**: Latest ESP-IDF framework

## Project Structure
```
esp32s3-modbus-rtu/
├── src/
│   ├── main.c              # Application entry point
│   ├── modbus.c            # Modbus RTU implementation
│   ├── wifi.c              # WiFi implementation
│   └── mqtt.c              # MQTT implementation
├── include/
│   ├── modbus.h            # Modbus RTU API
│   ├── wifi.h              # WiFi API
│   └── mqtt.h              # MQTT API
├── docs/README.md          # Documentation
├── CMakeLists.txt          # Build configuration
├── sdkconfig.defaults      # Default ESP-IDF config
├── required_components.json
├── LICENSE
└── README.md
```

## Getting Started
1. Ensure ESP-IDF v5.4.4 is installed and configured
2. Build: `idf.py build`
3. Flash: `idf.py -p /dev/ttyUSB0 flash monitor`

## Components
- **Modbus RTU**: Initialize with `modbus_init()`
- **WiFi**: Initialize with `wifi_init()`, connect with `wifi_connect(ssid, password)`
- **MQTT**: Initialize with `mqtt_init()`, connect with `mqtt_connect()`

---
Created by Qwythos (Empero AI)
