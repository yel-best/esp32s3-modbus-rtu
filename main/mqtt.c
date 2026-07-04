/**
 * @file mqtt.c
 * @brief MQTT implementation for ESP32-S3
 *
 * Provides MQTT client support based on the esp-mqtt component.
 */

#include <string.h>
#include "esp_log.h"
#include "esp_err.h"
#include "mqtt_client.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "mqtt.h"

#define TAG "MQTT"

#define MQTT_DEFAULT_BROKER_URI "mqtt://broker.empero.org:1883"
#define MQTT_CLIENT_ID          "esp32s3-modbus-rtu"
#define MQTT_KEEPALIVE_SEC      60

static esp_mqtt_client_handle_t mqtt_client = NULL;
static mqtt_event_data_handler_t data_handler = NULL;
static TaskHandle_t mqtt_task_handle = NULL;
static bool mqtt_initialized = false;
static bool mqtt_connected = false;
static char mqtt_broker_uri[128] = MQTT_DEFAULT_BROKER_URI;

/**
 * @brief Load MQTT broker URI from NVS (namespace "mqtt", key "broker_uri")
 */
static void mqtt_load_config_from_nvs(void)
{
    nvs_handle_t nvs;
    esp_err_t err = nvs_open("mqtt", NVS_READONLY, &nvs);
    if (err != ESP_OK) {
        ESP_LOGI(TAG, "No MQTT config in NVS, using default broker");
        return;
    }

    size_t len = sizeof(mqtt_broker_uri);
    err = nvs_get_str(nvs, "broker_uri", mqtt_broker_uri, &len);
    if (err != ESP_OK) {
        strlcpy(mqtt_broker_uri, MQTT_DEFAULT_BROKER_URI, sizeof(mqtt_broker_uri));
    }
    nvs_close(nvs);
}

/**
 * @brief MQTT event handler
 */
static void mqtt_event_handler(void *handler_args, esp_event_base_t base,
                               int32_t event_id, void *event_data)
{
    esp_mqtt_event_handle_t event = (esp_mqtt_event_handle_t)event_data;

    switch ((esp_mqtt_event_id_t)event_id) {
    case MQTT_EVENT_CONNECTED:
        ESP_LOGI(TAG, "MQTT connected");
        mqtt_connected = true;
        break;
    case MQTT_EVENT_DISCONNECTED:
        ESP_LOGW(TAG, "MQTT disconnected");
        mqtt_connected = false;
        break;
    case MQTT_EVENT_SUBSCRIBED:
        ESP_LOGI(TAG, "Subscribed, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_UNSUBSCRIBED:
        ESP_LOGI(TAG, "Unsubscribed, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_DATA:
        ESP_LOGI(TAG, "TOPIC=%.*s MSG=%.*s",
                 event->topic_len, event->topic,
                 event->data_len, event->data);
        if (data_handler != NULL) {
            data_handler(event->topic, event->topic_len,
                         event->data, event->data_len);
        }
        break;
    case MQTT_EVENT_ERROR:
        ESP_LOGE(TAG, "MQTT error");
        break;
    default:
        break;
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

    mqtt_load_config_from_nvs();

    const esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = mqtt_broker_uri,
        .credentials.client_id = MQTT_CLIENT_ID,
        .session.keepalive = MQTT_KEEPALIVE_SEC,
    };

    mqtt_client = esp_mqtt_client_init(&mqtt_cfg);
    if (mqtt_client == NULL) {
        ESP_LOGE(TAG, "Failed to init MQTT client");
        return;
    }

    ESP_ERROR_CHECK(esp_mqtt_client_register_event(mqtt_client, ESP_EVENT_ANY_ID,
                                                   mqtt_event_handler, NULL));

    mqtt_initialized = true;
    ESP_LOGI(TAG, "MQTT initialized (broker: %s)", mqtt_broker_uri);
}

/**
 * @brief Connect to MQTT broker (starts the client task)
 */
esp_err_t mqtt_connect(void)
{
    if (!mqtt_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    ESP_LOGI(TAG, "Connecting to MQTT broker %s...", mqtt_broker_uri);
    return esp_mqtt_client_start(mqtt_client);
}

/**
 * @brief Disconnect from MQTT broker (stops the client task)
 */
esp_err_t mqtt_disconnect(void)
{
    if (!mqtt_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    ESP_LOGI(TAG, "Disconnecting from MQTT...");
    esp_err_t err = esp_mqtt_client_stop(mqtt_client);
    mqtt_connected = false;
    return err;
}

/**
 * @brief Publish a message to an MQTT topic
 */
esp_err_t mqtt_publish(const char *topic, const void *data, size_t data_len)
{
    if (!mqtt_initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    if (topic == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    int msg_id = esp_mqtt_client_publish(mqtt_client, topic,
                                         (const char *)data, (int)data_len,
                                         1 /* QoS */, 0 /* retain */);
    if (msg_id < 0) {
        ESP_LOGE(TAG, "Failed to publish to %s", topic);
        return ESP_FAIL;
    }
    ESP_LOGD(TAG, "Published to %s, msg_id=%d", topic, msg_id);
    return ESP_OK;
}

/**
 * @brief Subscribe to an MQTT topic
 */
esp_err_t mqtt_subscribe(const char *topic, mqtt_event_data_handler_t handler)
{
    if (!mqtt_initialized || !mqtt_connected) {
        return ESP_ERR_INVALID_STATE;
    }
    if (topic == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    data_handler = handler;

    int msg_id = esp_mqtt_client_subscribe(mqtt_client, topic, 1 /* QoS */);
    if (msg_id < 0) {
        ESP_LOGE(TAG, "Failed to subscribe to %s", topic);
        return ESP_FAIL;
    }
    ESP_LOGI(TAG, "Subscribed to %s, msg_id=%d", topic, msg_id);
    return ESP_OK;
}

/**
 * @brief Unsubscribe from an MQTT topic
 */
esp_err_t mqtt_unsubscribe(const char *topic)
{
    if (!mqtt_initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    if (topic == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    int msg_id = esp_mqtt_client_unsubscribe(mqtt_client, topic);
    if (msg_id < 0) {
        ESP_LOGE(TAG, "Failed to unsubscribe from %s", topic);
        return ESP_FAIL;
    }
    ESP_LOGI(TAG, "Unsubscribed from %s, msg_id=%d", topic, msg_id);
    return ESP_OK;
}

/**
 * @brief Run MQTT task (stub - implement actual task logic here)
 */
static void mqtt_task(void *arg)
{
    ESP_LOGI(TAG, "MQTT task started");
    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

/**
 * @brief Start MQTT task
 */
void mqtt_start(void)
{
    if (!mqtt_initialized) {
        ESP_LOGW(TAG, "MQTT not initialized, task not started");
        return;
    }
    if (xTaskCreate(mqtt_task, "MQTT", 4096, NULL, 3,
                    &mqtt_task_handle) != pdPASS) {
        ESP_LOGE(TAG, "Failed to create MQTT task");
    }
}
