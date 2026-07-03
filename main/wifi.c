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
#include "esp_mac.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "wifi.h"

#define TAG "WIFI"

#define WIFI_CONNECT_TIMEOUT_MS 10000
#define WIFI_CONNECTED_BIT      BIT0
#define WIFI_FAIL_BIT           BIT1

static esp_netif_t *sta_netif = NULL;
static esp_netif_t *ap_netif = NULL;
static EventGroupHandle_t wifi_event_group = NULL;
static TaskHandle_t wifi_task_handle = NULL;
static bool wifi_initialized = false;

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
        xEventGroupSetBits(wifi_event_group, WIFI_FAIL_BIT);
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *ip_event = (ip_event_got_ip_t *)event_data;
        ESP_LOGI(TAG, "WiFi got IP: " IPSTR, IP2STR(&ip_event->ip_info.ip));
        xEventGroupSetBits(wifi_event_group, WIFI_CONNECTED_BIT);
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_AP_START) {
        ESP_LOGI(TAG, "WiFi AP started");
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_AP_STACONNECTED) {
        wifi_event_ap_staconnected_t *event = (wifi_event_ap_staconnected_t *)event_data;
        ESP_LOGI(TAG, "Station " MACSTR " joined AP", MAC2STR(event->mac));
    }
}

/**
 * @brief Initialize WiFi subsystem (STA mode)
 *
 * Requires nvs_flash_init(), esp_netif_init() and
 * esp_event_loop_create_default() to have been called (done in app_main).
 */
void wifi_init(void)
{
    if (wifi_initialized) {
        ESP_LOGW(TAG, "WiFi already initialized");
        return;
    }

    ESP_LOGI(TAG, "Initializing WiFi...");

    wifi_event_group = xEventGroupCreate();
    sta_netif = esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
                                               &wifi_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP,
                                               &wifi_event_handler, NULL));

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());

    wifi_initialized = true;
    ESP_LOGI(TAG, "WiFi initialized successfully");
}

/**
 * @brief Connect to WiFi access point
 */
esp_err_t wifi_connect(const char *ssid, const char *password)
{
    if (!wifi_initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    if (ssid == NULL || strlen(ssid) >= sizeof(((wifi_sta_config_t *)0)->ssid)) {
        ESP_LOGE(TAG, "Invalid SSID");
        return ESP_ERR_INVALID_ARG;
    }
    if (password == NULL ||
        strlen(password) >= sizeof(((wifi_sta_config_t *)0)->password)) {
        ESP_LOGE(TAG, "Invalid password");
        return ESP_ERR_INVALID_ARG;
    }

    ESP_LOGI(TAG, "Connecting to WiFi: %s", ssid);

    wifi_config_t wifi_config = {0};
    strlcpy((char *)wifi_config.sta.ssid, ssid, sizeof(wifi_config.sta.ssid));
    strlcpy((char *)wifi_config.sta.password, password,
            sizeof(wifi_config.sta.password));

    xEventGroupClearBits(wifi_event_group, WIFI_CONNECTED_BIT | WIFI_FAIL_BIT);
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_connect());

    EventBits_t bits = xEventGroupWaitBits(wifi_event_group,
                                           WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
                                           pdFALSE, pdFALSE,
                                           pdMS_TO_TICKS(WIFI_CONNECT_TIMEOUT_MS));
    if (bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI(TAG, "Connected to WiFi: %s", ssid);
        return ESP_OK;
    }

    ESP_LOGE(TAG, "Failed to connect to WiFi: %s", ssid);
    return ESP_FAIL;
}

/**
 * @brief Start WiFi SoftAP mode
 */
esp_err_t wifi_start_ap(const char *ssid, const char *password)
{
    if (!wifi_initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    if (ssid == NULL || strlen(ssid) >= sizeof(((wifi_ap_config_t *)0)->ssid)) {
        ESP_LOGE(TAG, "Invalid SSID");
        return ESP_ERR_INVALID_ARG;
    }
    if (password == NULL || strlen(password) < 8 ||
        strlen(password) >= sizeof(((wifi_ap_config_t *)0)->password)) {
        ESP_LOGE(TAG, "Invalid password (WPA2 requires 8-63 chars)");
        return ESP_ERR_INVALID_ARG;
    }

    ESP_LOGI(TAG, "Starting WiFi AP: %s", ssid);

    if (ap_netif == NULL) {
        ap_netif = esp_netif_create_default_wifi_ap();
    }

    wifi_config_t ap_config = {
        .ap = {
            .channel = 1,
            .max_connection = 4,
            .authmode = WIFI_AUTH_WPA2_PSK,
        },
    };
    strlcpy((char *)ap_config.ap.ssid, ssid, sizeof(ap_config.ap.ssid));
    ap_config.ap.ssid_len = strlen(ssid);
    strlcpy((char *)ap_config.ap.password, password, sizeof(ap_config.ap.password));

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &ap_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    return ESP_OK;
}

/**
 * @brief Get current WiFi mode
 */
wifi_mode_t wifi_get_mode(void)
{
    wifi_mode_t mode = WIFI_MODE_NULL;
    if (wifi_initialized) {
        esp_wifi_get_mode(&mode);
    }
    return mode;
}

/**
 * @brief Run WiFi task (stub - implement actual task logic here)
 */
static void wifi_task(void *arg)
{
    ESP_LOGI(TAG, "WiFi task started");
    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

/**
 * @brief Scan for available WiFi networks (blocking)
 */
esp_err_t wifi_scan(wifi_ap_record_t *results, size_t max_results)
{
    if (!wifi_initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    if (results == NULL || max_results == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    ESP_LOGI(TAG, "Scanning for WiFi networks...");

    esp_err_t err = esp_wifi_scan_start(NULL, true /* block until done */);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Failed to start scan: %s", esp_err_to_name(err));
        return err;
    }

    uint16_t num_results = (uint16_t)max_results;
    err = esp_wifi_scan_get_ap_records(&num_results, results);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Failed to get scan results: %s", esp_err_to_name(err));
        return err;
    }

    ESP_LOGI(TAG, "Found %u networks (requested max: %zu)",
             num_results, max_results);
    return ESP_OK;
}

/**
 * @brief Start WiFi task
 */
void wifi_start(void)
{
    if (!wifi_initialized) {
        ESP_LOGW(TAG, "WiFi not initialized, task not started");
        return;
    }
    if (xTaskCreate(wifi_task, "WIFI", 2048, NULL, 3,
                    &wifi_task_handle) != pdPASS) {
        ESP_LOGE(TAG, "Failed to create WiFi task");
    }
}
