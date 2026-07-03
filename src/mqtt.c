/**
 * @file mqtt.c
 * @brief MQTT implementation for ESP32-S3
 * 
 * Provides MQTT client support with broker communication.
 */

#include <string.h>
#include "esp_log.h"
#include "esp_err.h"
#include "mqtt.h"
#include "esp_netif.h"
#include "esp_event.h"
#include "nvs_flash.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define TAG "MQTT"
#define MQTT_MAX_TOPIC_LEN 64
#define MQTT_MAX_MESSAGE_LEN 256
#define MQTT_MAX_PACKET_LEN 1024

static TaskHandle_t mqtt_task_handle = NULL;
static bool mqtt_initialized = false;
static bool mqtt_connected = false;

/**
 * @brief MQTT broker configuration
 */
typedef struct {
    const char *broker_addr;
    int broker_port;
    const char *client_id;
    const char *username;
    const char *password;
} mqtt_config_t;

static char mqtt_nvs_config[64];

/**
 * @brief Load MQTT configuration from NVS
 */
static esp_err_t mqtt_load_config_from_nvs(void)
{
    if (nvs_flash_init() != ESP_OK) {
        return ESP_FAIL;
    }
    
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open("mqtt", NVS_READONLY, &nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "NVS open failed: %s", esp_err_to_name(err));
        return err;
    }
    
    size_t len = sizeof(mqtt_nvs_config) - 1;
    err = nvs_get_str(nvs_handle, "broker_addr", mqtt_nvs_config, &len);
    if (err == ESP_OK) {
        // Config loaded successfully
    } else if (err == ESP_ERR_NOT_FOUND) {
        mqtt_nvs_config[0] = '\0';  // Use default
    } else {
        ESP_LOGW(TAG, "Failed to read broker_addr: %s", esp_err_to_name(err));
        mqtt_nvs_config[0] = '\0';
    }
    
    nvs_close(nvs_handle);
    return ESP_OK;
}

static mqtt_config_t default_mqtt_config = {
    .broker_addr = "tcp://broker.empero.org",
    .broker_port = 1883,
    .client_id = "esp32s3-modbus-rtu-qwythos",
    .username = NULL,
    .password = NULL
};

/**
 * @brief MQTT event handler
 */
static void mqtt_event_handler(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data)
{
    if (event_base == MQTT_EVENT_CONNECTED) {
        ESP_LOGI(TAG, "MQTT Connected");
        mqtt_connected = true;
    } else if (event_base == MQTT_EVENT_DISCONNECTED) {
        ESP_LOGW(TAG, "MQTT Disconnected");
        mqtt_connected = false;
    } else if (event_base == MQTT_EVENT_ERROR) {
        esp_err_t error = *(esp_err_t *)event_data;
        ESP_LOGE(TAG, "MQTT Error: %s", esp_err_to_name(error));
    } else if (event_base == MQTT_EVENT_DATA && event_id == MQTT_EVENT_DATA) {
        mqtt_event_data_t *data = (mqtt_event_data_t *)event_data;
        ESP_LOGI(TAG, "TOPIC=%.*s MSG=%.*s", data->topic_len, data->topic,
                 data->data_len, data->data);
    } else if (event_base == MQTT_EVENT_SUBSCRIBED) {
        mqtt_subscription_id_t *subid = (mqtt_subscription_id_t *)event_data;
        ESP_LOGI(TAG, "Subscribed: %d", subid->topic_id);
    } else if (event_base == MQTT_EVENT_UNSUBSCRIBED) {
        mqtt_subscription_id_t *subid = (mqtt_subscription_id_t *)event_data;
        ESP_LOGI(TAG, "Unsubscribed: %d", subid->topic_id);
    }
}

/**
 * @brief Initialize MQTT subsystem
 */
void mqtt_init(void)
{
    if (mqtt_initialized) {
        ESP_LOGW(TAG, "MQTT already initialized");
        return;
    }

    ESP_LOGI(TAG, "Initializing MQTT...");

    // Load broker config from NVS
    mqtt_load_config_from_nvs();

    // Create default event loop
    EventHandle_t mqtt_event_handle;
    ESP_ERROR_CHECK(esp_event_loop_create_default(&mqtt_event_handle));

    // Create ESP-NETIF
    esp_netif_t *sta_netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    esp_netif_handle_t dummy_netif = { .type = ESP_NETIF_WIFI_STA, .handle = sta_netif };
    esp_netif_init(&dummy_netif);

    // Create MQTT netif
    esp_netif_t *mqtt_netif = esp_netif_createmqtt_with_default_config(
        "MQTT_DEF",
        &dummy_netif,
        NULL
    );

    // Initialize MQTT
    mqtt_init_config_t init_config = {
        .broker_addr = mqtt_nvs_config[0] ? mqtt_nvs_config : default_mqtt_config.broker_addr,
        .broker_port = default_mqtt_config.broker_port,
        .client_id = default_mqtt_config.client_id,
        .clean_session = true,
        .keepalive_ms = 60000,
    };

    ESP_ERROR_CHECK(mqtt_init(&init_config));

    // Set username/password if provided
    if (default_mqtt_config.username != NULL) {
        mqtt_set_username(default_mqtt_config.username);
    }
    if (default_mqtt_config.password != NULL) {
        mqtt_set_password(default_mqtt_config.password);
    }

    // Register event handlers
    ESP_ERROR_CHECK(esp_event_handler_register(MQTT_EVENT_BASE, MQTT_EVENT_ANY, &mqtt_event_handler, NULL));

    mqtt_initialized = true;
    ESP_LOGI(TAG, "MQTT initialized successfully");
}

/**
 * @brief Connect to MQTT broker
 */
esp_err_t mqtt_connect(void)
{
    if (!mqtt_initialized) {
        return ESP_ERR_NOT_INITIALIZED;
    }

    ESP_LOGI(TAG, "Connecting to MQTT broker...");

    esp_err_t err = mqtt_connect();
    if (err == ESP_OK) {
        mqtt_connected = true;
        ESP_LOGI(TAG, "MQTT connected to %s:%d",
                 default_mqtt_config.broker_addr,
                 default_mqtt_config.broker_port);
    } else {
        ESP_LOGE(TAG, "Failed to connect to MQTT broker: %s", esp_err_to_name(err));
    }

    return err;
}

/**
 * @brief Disconnect from MQTT broker
 */
esp_err_t mqtt_disconnect(void)
{
    if (!mqtt_initialized) {
        return ESP_ERR_NOT_INITIALIZED;
    }

    ESP_LOGI(TAG, "Disconnecting from MQTT...");
    esp_err_t err = mqtt_disconnect();
    mqtt_connected = false;
    return err;
}

/**
 * @brief Publish a message to an MQTT topic
 */
esp_err_t mqtt_publish(const char *topic, const void *data, size_t data_len)
{
    if (!mqtt_initialized) {
        return ESP_ERR_NOT_INITIALIZED;
    }

    ESP_LOGD(TAG, "Publishing to topic: %s", topic);
    
    esp_err_t err = mqtt_publish_data(topic, data, data_len);
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "Message published successfully");
    } else {
        ESP_LOGE(TAG, "Failed to publish: %s", esp_err_to_name(err));
    }
    
    return err;
}

/**
 * @brief Subscribe to an MQTT topic
 */
esp_err_t mqtt_subscribe(const char *topic, mqtt_event_data_handler_t handler)
{
    if (!mqtt_initialized || !mqtt_connected) {
        return ESP_ERR_NOT_CONNECTED;
    }

    ESP_LOGI(TAG, "Subscribing to topic: %s", topic);
    
    esp_err_t err = mqtt_subscribe(topic, handler);
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "Subscription created successfully");
    } else {
        ESP_LOGE(TAG, "Failed to subscribe: %s", esp_err_to_name(err));
    }
    
    return err;
}

/**
 * @brief Unsubscribe from an MQTT topic
 */
esp_err_t mqtt_unsubscribe(const char *topic)
{
    if (!mqtt_initialized) {
        return ESP_ERR_NOT_INITIALIZED;
    }

    ESP_LOGI(TAG, "Unsubscribing from topic: %s", topic);
    
    esp_err_t err = mqtt_unsubscribe(topic);
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "Unsubscription completed successfully");
    } else {
        ESP_LOGE(TAG, "Failed to unsubscribe: %s", esp_err_to_name(err));
    }
    
    return err;
}

/**
 * @brief Subscribe to an MQTT topic (duplicate removed - kept only the first implementation)
 */

/**
 * @brief Unsubscribe from an MQTT topic (duplicate removed - kept only the first implementation)
 */

/**
 * @brief Run MQTT task (stub - implement actual task logic here)
 */
static void mqtt_task(void *arg)
{
    ESP_LOGI(TAG, "MQTT task started");
    // TODO: Implement MQTT task loop with proper event processing
    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(1000));  // Process periodically
    }
}

/**
 * @brief Start MQTT task
 */
void mqtt_start(void)
{
    if (mqtt_initialized) {
        ESP_ERROR_CHECK(xTaskCreate(mqtt_task, "MQTT", 4096, NULL, 3, &mqtt_task_handle));
    }
}
