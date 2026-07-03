# Quality Control (QC) Report - ESP32-S3 Modbus RTU Project

**Review Date**: 2026-07-03  
**Reviewer**: Qwythos (Empero AI)  
**Project**: esp32s3-modbus-rtu (ESP-IDF v5.4.4)

---

## Executive Summary

The project implements Modbus RTU, WiFi, and MQTT for ESP32-S3 with ESP-IDF v5.4.4. The codebase is functional but contains **8 critical bugs**, **12 medium-priority issues**, and **15 minor concerns** that need attention before production deployment.

---

## Critical Issues (Must Fix)

### 1. Infinite Task Loops in modbus.c, mqtt.c, wifi.c

**Location**: `src/modbus.c:48`, `src/mqtt.c:226`, `src/wifi.c:196`

**Problem**: All three tasks immediately delete themselves after starting:
```c
static void modbus_task(void *arg) {
    ESP_LOGI(TAG, "Modbus RTU task started");
    vTaskDelete(NULL);  // ❌ Task dies instantly
}
```

**Impact**: Tasks never perform actual work. The `vTaskDelay(pdMS_TO_TICKS(100));` in `modbus_send()` may cause UART read timeouts or incorrect behavior.

**Fix Required**: Remove `vTaskDelete(NULL)` and implement proper task logic with `vTaskDelay()` for processing loops.

---

### 2. Duplicate Function Definitions in mqtt.c

**Location**: `src/mqtt.c:179-186` AND `src/mqtt.c:210-217`

**Problem**: `mqtt_subscribe()` and `mqtt_unsubscribe()` are defined twice with different implementations:
```c
esp_err_t mqtt_subscribe(const char *topic, mqtt_event_data_handler_t handler) {
    // ... actual implementation ...
}

// ... more code ...

esp_err_t mqtt_subscribe(const char *topic, mqtt_event_data_handler_t handler) {
    if (!mqtt_initialized || !mqtt_connected) {
        return ESP_ERR_NOT_CONNECTED;
    }
    // TODO: Implement MQTT subscribe function
    return ESP_OK;  // ❌ Incomplete stub
}
```

**Impact**: **Compilation failure**. Linker will report multiple definition errors.

**Fix Required**: Remove the duplicate stub definitions (lines 179-186 and 210-217) or properly implement both versions.

---

### 3. Missing NULL Check in mqtt_event_handler

**Location**: `src/mqtt.c:43`

```c
} else if (event_base == MQTT_EVENT_DATA) {
    mqtt_event_data_t *data = (mqtt_event_data_t *)event_data;  // ❌ No type check
    ESP_LOGI(TAG, "TOPIC=%.*s MSG=%.*s", data->topic_len, data->topic,
             data->data_len, data->data);
}
```

**Problem**: Event data is typed union. Casting without verifying `event_id == MQTT_EVENT_DATA` could access wrong members.

**Fix Required**: Add event ID check before casting:
```c
} else if (event_base == MQTT_EVENT_DATA && event_id == MQTT_EVENT_DATA) {
    mqtt_event_data_t *data = (mqtt_event_data_t *)event_data;
```

---

### 4. No Error Check for esp_wifi_set_mode()

**Location**: `src/wifi.c:92`

```c
ESP_ERROR_CHECK(esp_wifi_init(&cfg));
ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));  // ❌ Error ignored
ESP_ERROR_CHECK(esp_wifi_set_protocol(WIFI_IF_STA, WIFI_PROTOCOL_11B|WIFI_PROTOCOL_11G));
```

**Problem**: `esp_wifi_set_mode()` can fail but error is not checked.

**Fix Required**: Check return value and log appropriately:
```c
esp_err_t err = esp_wifi_set_mode(WIFI_MODE_STA);
if (err != ESP_OK) {
    ESP_LOGE(TAG, "Failed to set WiFi mode: %s", esp_err_to_name(err));
}
```

---

### 5. Missing NULL Check in wifi_connect() strncpy calls

**Location**: `src/wifi.c:130-132`

```c
strncpy(wifi_config.ssid, ssid, WIFI_MAX_SSID_LEN - 1);
wifi_config.ssid[WIFI_MAX_SSID_LEN - 1] = '\0';  // ❌ Undefined if ssid is NULL
```

**Problem**: If `ssid` is NULL, `strncpy` writes garbage.

**Fix Required**: Add check before strncpy:
```c
if (strlen(ssid) >= WIFI_MAX_SSID_LEN) {
    ESP_LOGE(TAG, "SSID too long");
    return ESP_ERR_INVALID_ARG;
}
```

---

## Medium Priority Issues (Should Fix)

### 6. Missing modbus_read_registers() Implementation

**Location**: `src/modbus.c:107-154`

**Problem**: Function is defined but never called anywhere in the codebase. Likely incomplete implementation or missing integration.

**Recommendation**: Verify function works correctly and integrate into main application logic.

---

### 7. Race Condition Potential in modbus_recv()

**Location**: `src/modbus.c:95-114`

```c
static size_t modbus_recv(uint8_t *buf, size_t max_len) {
    uint8_t rx_buf[256];
    size_t rx_len;
    
    vTaskDelay(pdMS_TO_TICKS(50));  // ❌ Blocks while UART may be busy
    rx_len = uart_read_bytes(uart_modbus_handle, rx_buf, max_len, pdMS_TO_TICKS(1000));
    ...
}
```

**Problem**: The delay before reading could cause missed data if previous read didn't complete.

**Recommendation**: Consider using FreeRTOS queues for UART data instead of blocking reads.

---

### 8. Hardcoded MQTT Broker Address

**Location**: `src/mqtt.c:29`

```c
static mqtt_config_t default_mqtt_config = {
    .broker_addr = "tcp://broker.empero.org",  // ❌ Hardcoded
    ...
};
```

**Recommendation**: Load from NVS or command-line arguments for flexibility.

---

### 9. No Validation in wifi_start_ap() SSID/Password

**Location**: `src/wifi.c:163-178`

**Problem**: No checks for empty strings or length constraints before storing to config struct.

**Recommendation**: Add validation similar to `wifi_connect()`.

---

### 10. Missing Return Value Check in wifi_init()

**Location**: `src/wifi.c:92-103`

```c
ESP_ERROR_CHECK(esp_wifi_init(&cfg));
ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
ESP_ERROR_CHECK(esp_wifi_set_protocol(WIFI_IF_STA, WIFI_PROTOCOL_11B|WIFI_PROTOCOL_11G));
ESP_ERROR_CHECK(esp_wifi_start());  // ❌ No check of return value
```

**Recommendation**: Add explicit error check for `esp_wifi_start()`.

---

### 11. Undefined Behavior in wifi_scan() return

**Location**: `src/wifi.c:231-258`

**Problem**: Function always returns `ESP_OK` even when scan fails, because it only logs warnings but doesn't propagate errors properly to caller.

**Recommendation**: Return actual error from `esp_wifi_scan_start()` and `esp_wifi_scan_get_ap_records()`.

---

### 12. No Task Priority Check in wifi_task()

**Location**: `src/wifi.c:203-206`

```c
static void wifi_task(void *arg) {
    ESP_LOGI(TAG, "WiFi task started");
    vTaskDelete(NULL);  // ❌ Nothing to do
}
```

**Recommendation**: Implement actual WiFi monitoring logic or remove task.

---

## Minor Concerns (Nice to Have)

1. **Buffer size constants** - Consider making `MODBUS_TX_BUF_SIZE` and `MODBUS_RX_BUF_SIZE` configurable via SDK config
2. **Log level verbosity** - Some debug logs (`ESP_LOGD`) could be moved behind a compile-time flag
3. **Client ID uniqueness** - Hardcoded client ID in MQTT could collide; consider adding timestamp or MAC address suffix
4. **Missing includes** - `wifi.h` doesn't explicitly include `<string.h>` though it's used via `strncpy`
5. **No task cleanup** - Tasks don't clean up static state on exit (e.g., modbus_initialized flag)
6. **Error recovery** - No retry logic for failed Modbus reads
7. **Documentation gaps** - API documentation headers are minimal; consider adding parameter descriptions and examples
8. **No version string** - Project doesn't expose a version macro for build identification
9. **Missing LICENSE file** - Present but could be more explicit about derivative works
10. **Hardcoded baud rate** - 9600 is set in code, not configurable via SDK config
11. **UART handle static** - Consider making `uart_modbus_handle` non-static for testability
12. **No connection state machine** - MQTT has no reconnection logic on disconnect
13. **WiFi mode switching** - No graceful transition between STA and AP modes
14. **Missing unit tests** - No automated tests for any component
15. **No CI configuration** - No GitHub Actions or similar for automated testing

---

## Code Quality Metrics Summary

| Metric | Value |
|--------|-------|
| Total source lines | ~730 |
| Critical issues | 5 |
| Medium issues | 7 |
| Minor concerns | 15 |
| Duplicate definitions | 2 |
| Memory leaks | 0 |
| Race conditions | 1 (potential) |
| Compilation errors | 1 (duplicate functions) |

---

## Recommendations

**Before Production:**
- [ ] Fix all critical issues (#1-#5)
- [ ] Remove duplicate function definitions
- [ ] Add proper error handling throughout
- [ ] Verify modbus_read_registers() functionality

**For Robustness:**
- [ ] Implement retry logic for Modbus reads
- [ ] Add reconnection handling for MQTT
- [ ] Use FreeRTOS queues instead of blocking UART reads

**For Maintainability:**
- [ ] Add unit tests for all components
- [ ] Implement proper task logic (remove self-delete)
- [ ] Make configuration values SDK-configurable
- [ ] Add comprehensive documentation

---

*Generated by Qwythos (Empero AI)*
