/**
 * @file modbus.h
 * @brief Modbus RTU header for ESP32-S3
 */

#ifndef MODBUS_H
#define MODBUS_H

#include "esp_err.h"

/**
 * @brief Initialize Modbus RTU interface
 */
void modbus_init(void);

/**
 * @brief Start Modbus RTU task
 */
void modbus_start(void);

/**
 * @brief Read a Modbus register value
 * @param address Register address (0-65535)
 * @param value Pointer to store the read value
 * @return ESP_OK on success, ESP_ERR_TIMEOUT if no response
 */
esp_err_t modbus_read_register(uint16_t address, uint16_t *value);

/**
 * @brief Write a Modbus register value
 * @param address Register address (0-65535)
 * @param value Value to write
 * @return ESP_OK on success, ESP_ERR_TIMEOUT if no response
 */
esp_err_t modbus_write_register(uint16_t address, uint16_t value);

/**
 * @brief Send a Modbus PDU (Protocol Data Unit)
 * @param fc Function code
 * @param addr Register address
 * @param data Data bytes for the request
 * @param data_len Length of data
 * @return ESP_OK on success, ESP_ERR_INVALID_SIZE if data too large
 */
esp_err_t modbus_send_pdu(uint8_t fc, uint16_t addr,
                         const uint8_t *data, size_t data_len);

/**
 * @brief Send a Modbus request and receive response
 * @param cmd Pointer to Modbus command buffer (max 256 bytes)
 * @param length Length of command
 * @return ESP_OK on success, ESP_ERR_TIMEOUT if no response
 */
esp_err_t modbus_send(uint8_t *cmd, size_t length);

#endif /* MODBUS_H */
