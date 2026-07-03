/**
 * @file main.c
 * @brief ESP32-S3 Modbus RTU / WiFi / MQTT Application Entry Point
 * 
 * Project: esp32s3-modbus-rtu (ESP-IDF v5.4.4)
 * Features: Modbus RTU, WiFi STA/SoftAP, MQTT client
 */

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "modbus.h"
#include "wifi.h"
#include "mqtt.h"

#define TAG "MODBUS_APP"

/**
 * @brief Application entry point
 * 
 * Initializes ESP-IDF subsystems and starts the application.
 */
void app_main(void)
{
    esp_log_level_set(TAG, ESP_LOG_INFO);
    
    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    printf("\n=== ESP32-S3 Modbus RTU Project ===\n");
    printf("ESP-IDF v5.4.4\n");
    printf("Components: Modbus RTU, WiFi, MQTT\n");
    printf("=================================\n\n");

    /* Initialize Modbus RTU component */
    modbus_init();
    
    /* Initialize WiFi component */
    wifi_init();
    
    /* Initialize MQTT component */
    mqtt_init();

    printf("All components initialized.\n\n");
}
