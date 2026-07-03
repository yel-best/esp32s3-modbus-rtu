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
├── main/                   # Main application component
│   ├── CMakeLists.txt      # Component registration
│   ├── main.c              # Application entry point
│   ├── modbus.c            # Modbus RTU implementation
│   ├── wifi.c              # WiFi implementation
│   ├── mqtt.c              # MQTT implementation
│   └── include/
│       ├── modbus.h        # Modbus RTU API
│       ├── wifi.h          # WiFi API
│       └── mqtt.h          # MQTT API
├── tests/                  # Unit tests (not yet wired into the build)
├── docs/README.md          # Documentation
├── .github/workflows/ci.yml # GitHub Actions build & release pipeline
├── CMakeLists.txt          # Top-level ESP-IDF project file
├── sdkconfig.defaults      # Default ESP-IDF config
├── LICENSE
└── README.md
```

## Getting Started
1. Ensure ESP-IDF v5.4.x is installed and configured
2. Set target: `idf.py set-target esp32s3`
3. Build: `idf.py build`
4. Flash: `idf.py -p /dev/ttyUSB0 flash monitor`

## CI/CD
Every push / PR to `main` or `develop` builds the firmware on GitHub Actions
(ESP-IDF v5.4.4) and uploads `esp32s3_modbus_rtu.bin` / `.elf` / `.map`,
bootloader and partition table as artifacts. Pushing a `v*` tag additionally
packages them into a `.tar.gz` release archive.

## Components
- **Modbus RTU**: Initialize with `modbus_init()`
- **WiFi**: Initialize with `wifi_init()`, connect with `wifi_connect(ssid, password)`
- **MQTT**: Initialize with `mqtt_init()`, connect with `mqtt_connect()`

---
Created by Qwythos (Empero AI)
