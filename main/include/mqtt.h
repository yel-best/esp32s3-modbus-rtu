/**
 * @file mqtt.h
 * @brief MQTT header for ESP32-S3
 */

#ifndef MQTT_H
#define MQTT_H

#include <stddef.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Callback for incoming MQTT messages
 * @param topic Topic the message arrived on (not null-terminated)
 * @param topic_len Length of topic
 * @param data Message payload (not null-terminated)
 * @param data_len Length of payload
 */
typedef void (*mqtt_event_data_handler_t)(const char *topic, size_t topic_len,
                                          const char *data, size_t data_len);

/**
 * @brief Initialize MQTT subsystem
 */
void mqtt_init(void);

/**
 * @brief Start MQTT task
 */
void mqtt_start(void);

/**
 * @brief Connect to MQTT broker
 * @return ESP_OK on success, ESP_ERR_INVALID_STATE if not initialized
 */
esp_err_t mqtt_connect(void);

/**
 * @brief Disconnect from MQTT broker
 * @return ESP_OK on success
 */
esp_err_t mqtt_disconnect(void);

/**
 * @brief Publish a message to an MQTT topic
 * @param topic Topic name (null-terminated string)
 * @param data Message payload
 * @param data_len Length of payload
 * @return ESP_OK on success, ESP_ERR_INVALID_STATE if not initialized
 */
esp_err_t mqtt_publish(const char *topic, const void *data, size_t data_len);

/**
 * @brief Subscribe to an MQTT topic
 * @param topic Topic name (null-terminated string)
 * @param handler Callback function for incoming messages
 * @return ESP_OK on success, ESP_ERR_NOT_CONNECTED if not connected
 */
esp_err_t mqtt_subscribe(const char *topic, mqtt_event_data_handler_t handler);

/**
 * @brief Unsubscribe from an MQTT topic
 * @param topic Topic name (null-terminated string)
 * @return ESP_OK on success
 */
esp_err_t mqtt_unsubscribe(const char *topic);

#ifdef __cplusplus
}
#endif

#endif /* MQTT_H */
