/**
 * @file wifi.c
 * @brief WiFi implementation for ESP32-S3
 * 
 * Provides Wi-Fi connectivity support (STA and SoftAP modes).
 */

#include <string.h>
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "nvs_flash.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "wifi.h"

#define TAG "WIFI"
#define WIFI_MAX_SSID_LEN 32
#define WIFI_MAX_PASSWORD_LEN 64

static esp_netif_t *esp_netif_handle = NULL;
static wifi_mode_t current_wifi_mode = WIFI_MODE_NULL;
static EventHandle_t wifi_event_handle = NULL;
static TaskHandle_t wifi_task_handle = NULL;
static bool wifi_initialized = false;

/**
 * @brief WiFi configuration structure
 */
typedef struct {
    char ssid[WIFI_MAX_SSID_LEN];
    char password[WIFI_MAX_PASSWORD_LEN];
} wifi_config_t;

static wifi_config_t default_wifi_config = {
    .ssid = "ESP32S3-WiFi",
    .password = "password123"
};

/**
 * @brief WiFi event handler
 */
static void wifi_event_handler(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        ESP_LOGI(TAG, "WiFi STA started");
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        ESP_LOGW(TAG, "WiFi STA disconnected");
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *ip_event = (ip_event_got_ip_t *)event_data;
        ESP_LOGI(TAG, "WiFi got IP: %s", ip_event->ip_info.ip);
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_AP_START) {
        ESP_LOGI(TAG, "WiFi AP started");
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_AP_STACONNECTED) {
        ESP_LOGI(TAG, "WiFi AP connected: %d", *((uint8_t *)event_data));
    }
}

/**
 * @brief Initialize WiFi subsystem
 */
void wifi_init(void)
{
    if (wifi_initialized) {
        ESP_LOGW(TAG, "WiFi already initialized");
        return;
    }

    ESP_LOGI(TAG, "Initializing WiFi...");

    // Initialize NVS for WiFi configuration
    ESP_ERROR_CHECK(nvs_flash_init());

    // Create default event loop
    ESP_ERROR_CHECK(esp_event_loop_create_default(&wifi_event_handle));

    // Create ESP-NETIF
    ESP_ERROR_CHECK(esp_netif_create_default_wifi_sta());
    esp_netif_t *sta_netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    esp_netif_handle_t dummy_handle = { .type = ESP_NETIF_WIFI_STA, .handle = sta_netif };
    esp_netif_init(&dummy_handle);

    // Initialize WiFi
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_protocol(WIFI_IF_STA, WIFI_PROTOCOL_11B|WIFI_PROTOCOL_11G));
    ESP_ERROR_CHECK(esp_wifi_start());

    esp_netif_handle_t dummy_netif = { .type = ESP_NETIF_WIFI_STA, .handle = sta_netif };
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL));

    esp_netif_handle_t dummy_netif_ap = { .type = ESP_NETIF_WIFI_AP, .handle = NULL };
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_AP_START, &wifi_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_AP_STACONNECTED, &wifi_event_handler, NULL));

    esp_netif_init(&dummy_netif_ap);

    wifi_initialized = true;
    ESP_LOGI(TAG, "WiFi initialized successfully");
}

/**
 * @brief Connect to WiFi access point
 */
esp_err_t wifi_connect(const char *ssid, const char *password)
{
    if (!wifi_initialized) {
        return ESP_ERR_NOT_INITIALIZED;
    }

    ESP_LOGI(TAG, "Connecting to WiFi: %s", ssid);

    wifi_config_t wifi_config = {0};
    strncpy(wifi_config.ssid, ssid, WIFI_MAX_SSID_LEN - 1);
    wifi_config.ssid[WIFI_MAX_SSID_LEN - 1] = '\0';
    strncpy(wifi_config.password, password, WIFI_MAX_PASSWORD_LEN - 1);
    wifi_config.password[WIFI_MAX_PASSWORD_LEN - 1] = '\0';

    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_connect());

    // Wait for connection
    for (int i = 0; i < 30; i++) {
        if (esp_netif_get_ip_info(esp_netif_get_handle_from_ifkey("WIFI_STA_DEF")).ip.addr != 0) {
            ESP_LOGI(TAG, "Connected to WiFi");
            return ESP_OK;
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }

    ESP_LOGE(TAG, "Failed to connect to WiFi");
    return ESP_FAIL;
}

/**
 * @brief Start WiFi SoftAP mode
 */
esp_err_t wifi_start_ap(const char *ssid, const char *password)
{
    if (!wifi_initialized) {
        return ESP_ERR_NOT_INITIALIZED;
    }

    ESP_LOGI(TAG, "Starting WiFi AP: %s", ssid);

    wifi_config_t ap_config = {0};
    strncpy(ap_config.ssid, ssid, WIFI_MAX_SSID_LEN - 1);
    ap_config.ssid[WIFI_MAX_SSID_LEN - 1] = '\0';
    strncpy(ap_config.password, password, WIFI_MAX_PASSWORD_LEN - 1);
    ap_config.password[WIFI_MAX_PASSWORD_LEN - 1] = '\0';

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &ap_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    return ESP_OK;
}

/**
 * @brief Get current WiFi mode
 */
wifi_mode_t wifi_get_mode(void)
{
    return current_wifi_mode;
}

/**
 * @brief Run WiFi task
 */
static void wifi_task(void *arg)
{
    ESP_LOGI(TAG, "WiFi task started");
    vTaskDelete(NULL);
}

/**
 * @brief Start WiFi task
 */
void wifi_start(void)
{
    if (wifi_initialized) {
        ESP_ERROR_CHECK(xTaskCreate(wifi_task, "WIFI", 2048, NULL, 3, &wifi_task_handle));
    }
}
