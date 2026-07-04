# ESP32-S3 Modbus RTU Project - Memory

**Repository**: https://github.com/yel-best/esp32s3-modbus-rtu  
**Branch**: `main` (latest), `develop`  
**ESP-IDF Version**: v5.4.4

---

## 📋 Project Overview

An ESP32-S3 firmware project providing Modbus RTU serial communication, WiFi connectivity, and MQTT broker client functionality, all integrated with a robust CI/CD pipeline on GitHub Actions.

### Core Features
- **Modbus RTU**: Serial communication at 9600bps, function codes 03 (read) / 06 (write), CRC-16 validation
- **WiFi**: STA mode by default, SoftAP mode available via `wifi_start_ap()`
- **MQTT**: Client connection management, publish/subscribe with event handlers

### Project Structure
```
esp32s3-modbus-rtu/
├── main/                          # Main ESP-IDF component
│   ├── CMakeLists.txt            # Component registration
│   ├── include/                  # Public API headers (modbus.h, wifi.h, mqtt.h)
│   ├── main.c                    # Application entry point
│   ├── modbus.c                  # Modbus RTU implementation
│   ├── mqtt.c                    # MQTT client implementation
│   └── wifi.c                    # WiFi implementation
├── .github/workflows/ci.yml      # GitHub Actions CI/CD pipeline
├── sdkconfig.defaults            # ESP-IDF default config
├── README.md                     # Project documentation
└── PROJECT_MEMORY.md             # This file
```

---

## ⚙️ CI/CD Configuration

### Workflow Location
`.github/workflows/ci.yml`

### Trigger Events
| Event | Branches | Tags | Manual Dispatch |
|-------|----------|------|-----------------|
| Push | main, develop | v* (pushed) | ✅ |
| Pull Request | main, develop | — | — |
| Workflow Dispatch | — | — | ✅ |

### Build Job
1. **Checkout** with recursive submodules
2. **Install ESP-IDF** via `espressif/install-esp-idf-action@v1` (official action)
3. **Set target**: esp32s3
4. **Build** with `idf.py build`, output piped to `build_output.log`
5. **Surface errors as annotations** on failure (grep for error patterns)
6. **Run size info**: `idf.py size`
7. **Upload artifacts**: `esp-idf-build` containing:
   - `build/esp32s3_modbus_rtu.bin` (firmware binary)
   - `build/esp32s3_modbus_rtu.elf` (debug symbols)
   - `build/esp32s3_modbus_rtu.map` (memory map)
   - `build/bootloader/bootloader.bin`
   - `build/partition_table/partition-table.bin`
   - `build/flasher_args.json`

### Release Packaging Job
Triggered **only** when a tag matching `v*` is pushed:
1. Downloads build artifacts into `release/` directory
2. Creates tarball: `esp-idf-firmware-{tag}.tar.gz`
3. Uploads as artifact
4. Publishes GitHub Release with files and auto-generated notes

### Flash Job
Triggered only on push to `main` branch (not PRs):
- Downloads artifacts, reinstalls ESP-IDF
- Runs `idf.py flash -p /dev/ttyUSB0 monitor`

---

## 📝 Coding Standards & Practices

### ESP-IDF Conventions
- **Component layout**: `main/` directory with `CMakeLists.txt`, `include/`, and `.c` files
- **Header guards**: Always include `#ifndef MODBUS_H / #define ... / #endif`
- **ESP_ERROR_CHECK**: Used throughout for error propagation
- **Logging**: TAG macros (`#define TAG "MODBUS"`), appropriate ESP_LOG levels
- **FreeRTOS tasks**: Proper task handles tracked, vTaskDelay used for periodic work

### Modbus RTU Implementation
- UART0 configured at 9600bps, 8N1, no flow control
- CRC-16 Modbus calculation and validation
- Functions: read/write single register, read multiple registers, send PDU
- No slave mode (master/client only)

### MQTT Integration
- Default broker: `tcp://broker.empero.org:1883`
- Client ID: `esp32s3-modbus-rtu-qwythos`
- Event handlers for CONNECTED/DISCONNECTED/ERROR/DATA/SUBSCRIBED/UNSUBSCRIBED
- Publish/subscribe functions with proper error handling

### WiFi Module
- STA mode default, SoftAP via `wifi_start_ap(ssid, password)`
- Scan function available: `wifi_scan(results, max_results)`
- Connection retry loop with IP address check

---

## 🛠️ SDK Configuration

**File**: `sdkconfig.defaults`

| Symbol | Value | Description |
|--------|-------|-------------|
| CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ_240=y | 240 MHz | CPU frequency |
| CONFIG_FREERTOS_HZ=1000 | 1000 Hz | FreeRTOS tick rate |

### Build Commands
```bash
idf.py set-target esp32s3
idf.py build
idf.py flash -p /dev/ttyUSB0 monitor
idf.py test              # Run unit tests if any
idf.py size              # Memory usage report
```

---

## 📦 Build Artifacts

After successful build, the following files are generated in `build/esp32s3_modbus_rtu/`:

| File | Purpose | Size (approx.) |
|------|---------|----------------|
| `firmware.bin` | Bootable firmware image | ~50 KB |
| `firmware.elf` | ELF with debug symbols | ~150 KB |
| `.map` | Memory layout map | — |
| `bootloader.bin` | ESP32-S3 bootloader | 43 KB |
| `partition-table.bin` | Partition table | — |
| `flasher_args.json` | Flash arguments for ESP-PROG | — |

---

## 📊 CI/CD Workflow Summary

**Current state**: ✅ Fully configured and operational

- **ESP-IDF installation**: Official action (no manual steps)
- **Caching**: Enabled via `actions/cache@v4` on ESP-IDF directories and build output
- **Error reporting**: Build errors surfaced as GitHub Actions annotations
- **Release automation**: Tag-based packaging with GitHub Release creation
- **Testing**: Unit tests framework present (`src/test/`) but not yet wired into workflow

**Last CI update**: Commit `dd7f23a` — upgraded to official install action, added cache, release packaging

---

## 📌 Notes & Future Work

- Unit tests in `src/test/` exist but are not executed by the CI pipeline
- Documentation could be expanded with more detailed API references and usage examples
- Hardware integration guide (pin assignments, RS-485 wiring) would be valuable
- Consider adding `idf.py check` for static analysis in future workflow version

---

*Generated by Qwythos (Empero AI)*  
*Last updated: 2026-07-04*
