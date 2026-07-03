/**
 * @file modbus.h
 * @brief Modbus RTU header for ESP32-S3
 */

#ifndef MODBUS_H
#define MODBUS_H

#include <stddef.h>
#include <stdint.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize Modbus RTU interface
 */
void modbus_init(void);

/**
 * @brief Start Modbus RTU task
 */
void modbus_start(void);

/**
 * @brief Read a Modbus holding register value (function code 03)
 * @param address Register address (0-65535)
 * @param value Pointer to store the read value
 * @return ESP_OK on success, ESP_ERR_TIMEOUT if no response
 */
esp_err_t modbus_read_register(uint16_t address, uint16_t *value);

/**
 * @brief Read multiple Modbus holding registers (function code 03)
 * @param addr Start register address
 * @param values Array to store read values
 * @param count Number of registers to read (1-125)
 * @return ESP_OK on success, ESP_ERR_TIMEOUT if no response
 */
esp_err_t modbus_read_registers(uint16_t addr, uint16_t *values, size_t count);

/**
 * @brief Write a Modbus holding register value (function code 06)
 * @param address Register address (0-65535)
 * @param value Value to write
 * @return ESP_OK on success, ESP_ERR_TIMEOUT if no response
 */
esp_err_t modbus_write_register(uint16_t address, uint16_t value);

/**
 * @brief Send a Modbus PDU (slave address prepended, CRC appended)
 * @param fc Function code
 * @param addr Register address
 * @param data Data bytes for the request
 * @param data_len Length of data
 * @return ESP_OK on success, ESP_ERR_INVALID_SIZE if data too large
 */
esp_err_t modbus_send_pdu(uint8_t fc, uint16_t addr,
                          const uint8_t *data, size_t data_len);

/**
 * @brief Send a raw Modbus frame (CRC appended) and validate the response CRC
 * @param cmd Pointer to Modbus command buffer (max 254 bytes)
 * @param length Length of command without CRC
 * @return ESP_OK on success, ESP_ERR_TIMEOUT if no response
 */
esp_err_t modbus_send(uint8_t *cmd, size_t length);

#ifdef __cplusplus
}
#endif

#endif /* MODBUS_H */
