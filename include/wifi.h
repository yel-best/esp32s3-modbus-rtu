/**
 * @file wifi.h
 * @brief WiFi header for ESP32-S3
 */

#ifndef WIFI_H
#define WIFI_H

#include "esp_err.h"
#include "esp_wifi_types.h"
#include "esp_scan.h"
#include "freertos/FreeRTOS.h"

/**
 * @brief Initialize WiFi subsystem
 */
void wifi_init(void);

/**
 * @brief Start WiFi task
 */
void wifi_start(void);

/**
 * @brief Connect to WiFi access point
 * @param ssid Access point SSID
 * @param password Access point password
 * @return ESP_OK on success, ESP_FAIL on failure
 */
esp_err_t wifi_connect(const char *ssid, const char *password);

/**
 * @brief Start WiFi SoftAP mode
 * @param ssid AP SSID
 * @param password AP password
 * @return ESP_OK on success, ESP_ERR_NOT_INITIALIZED if WiFi not initialized
 */
esp_err_t wifi_start_ap(const char *ssid, const char *password);

/**
 * @brief Get current WiFi mode
 * @return Current WiFi mode (WIFI_MODE_STA, WIFI_MODE_AP, etc.)
 */
wifi_mode_t wifi_get_mode(void);

/**
 * @brief Scan for available WiFi networks
 * @param results Array to store scan results (max WIFI_SCAN_RESULTS_MAX entries)
 * @param max_results Maximum number of results to retrieve
 * @return ESP_OK on success, ESP_ERR_NOT_INITIALIZED if WiFi not initialized, ESP_FAIL if scan failed
 */
esp_err_t wifi_scan(wifi_ap_record_t *results, size_t max_results);

#endif /* WIFI_H */
