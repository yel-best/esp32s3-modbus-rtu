/**
 * @file modbus.c
 * @brief Modbus RTU master implementation for ESP32-S3
 *
 * Provides serial-based Modbus RTU communication over UART1.
 */

#include <string.h>
#include "esp_log.h"
#include "esp_err.h"
#include "driver/uart.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "modbus.h"

#define TAG "MODBUS"

#define MODBUS_UART_PORT      UART_NUM_1
#define MODBUS_UART_TX_PIN    17
#define MODBUS_UART_RX_PIN    18
#define MODBUS_BAUD_RATE      9600
#define MODBUS_SLAVE_ADDR     1
#define MODBUS_BUF_SIZE       256
#define MODBUS_RESPONSE_TIMEOUT_MS 500

static TaskHandle_t modbus_task_handle = NULL;
static bool modbus_initialized = false;

/**
 * @brief Calculate CRC16 for Modbus RTU (poly 0xA001, init 0xFFFF, LSB first)
 */
static uint16_t modbus_crc16(const uint8_t *data, size_t len)
{
    uint16_t crc = 0xFFFF;
    for (size_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (int j = 0; j < 8; j++) {
            if (crc & 0x0001) {
                crc = (crc >> 1) ^ 0xA001;
            } else {
                crc >>= 1;
            }
        }
    }
    return crc;
}

/**
 * @brief Initialize Modbus RTU serial interface
 */
void modbus_init(void)
{
    if (modbus_initialized) {
        ESP_LOGW(TAG, "Modbus already initialized");
        return;
    }

    ESP_LOGI(TAG, "Initializing Modbus RTU on UART%d...", MODBUS_UART_PORT);

    const uart_config_t uart_config = {
        .baud_rate = MODBUS_BAUD_RATE,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };

    ESP_ERROR_CHECK(uart_driver_install(MODBUS_UART_PORT,
                                        MODBUS_BUF_SIZE * 2, 0, 0, NULL, 0));
    ESP_ERROR_CHECK(uart_param_config(MODBUS_UART_PORT, &uart_config));
    ESP_ERROR_CHECK(uart_set_pin(MODBUS_UART_PORT,
                                 MODBUS_UART_TX_PIN, MODBUS_UART_RX_PIN,
                                 UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));

    modbus_initialized = true;
    ESP_LOGI(TAG, "Modbus RTU initialized on UART%d (%d bps)",
             MODBUS_UART_PORT, MODBUS_BAUD_RATE);
}

/**
 * @brief Run Modbus RTU task (stub - implement polling logic here)
 */
static void modbus_task(void *arg)
{
    ESP_LOGI(TAG, "Modbus RTU task started");
    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

/**
 * @brief Start Modbus RTU task
 */
void modbus_start(void)
{
    if (!modbus_initialized) {
        ESP_LOGW(TAG, "Modbus not initialized, task not started");
        return;
    }
    if (xTaskCreate(modbus_task, "MODBUS", 2048, NULL, 3,
                    &modbus_task_handle) != pdPASS) {
        ESP_LOGE(TAG, "Failed to create Modbus task");
    }
}

/**
 * @brief Send a frame (CRC appended) and receive the raw response
 * @param tx      Frame without CRC
 * @param tx_len  Frame length without CRC
 * @param rx      Response buffer, may be NULL to discard (CRC stripped)
 * @param rx_size Size of response buffer
 * @param rx_len  Out: response length without CRC, may be NULL
 */
static esp_err_t modbus_transact(const uint8_t *tx, size_t tx_len,
                                 uint8_t *rx, size_t rx_size, size_t *rx_len)
{
    uint8_t tx_buf[MODBUS_BUF_SIZE];
    uint8_t rx_buf[MODBUS_BUF_SIZE];

    if (!modbus_initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    if (tx_len == 0 || tx_len + 2 > sizeof(tx_buf)) {
        return ESP_ERR_INVALID_SIZE;
    }

    memcpy(tx_buf, tx, tx_len);
    uint16_t crc = modbus_crc16(tx_buf, tx_len);
    tx_buf[tx_len] = crc & 0xFF;        /* CRC low byte first per Modbus RTU */
    tx_buf[tx_len + 1] = crc >> 8;

    uart_flush_input(MODBUS_UART_PORT);

    int written = uart_write_bytes(MODBUS_UART_PORT, tx_buf, tx_len + 2);
    if (written != (int)(tx_len + 2)) {
        ESP_LOGW(TAG, "UART write failed");
        return ESP_FAIL;
    }
    /* 256-byte frame at 9600 bps takes ~270 ms to shift out */
    if (uart_wait_tx_done(MODBUS_UART_PORT, pdMS_TO_TICKS(500)) != ESP_OK) {
        ESP_LOGW(TAG, "UART TX drain timeout");
        return ESP_ERR_TIMEOUT;
    }

    int read = uart_read_bytes(MODBUS_UART_PORT, rx_buf, sizeof(rx_buf),
                               pdMS_TO_TICKS(MODBUS_RESPONSE_TIMEOUT_MS));
    if (read <= 0) {
        ESP_LOGW(TAG, "No response from Modbus device");
        return ESP_ERR_TIMEOUT;
    }
    if (read < 4) {  /* addr + fc + CRC16 minimum */
        ESP_LOGW(TAG, "Response too short: %d bytes", read);
        return ESP_FAIL;
    }

    uint16_t rx_crc = (uint16_t)rx_buf[read - 2] | ((uint16_t)rx_buf[read - 1] << 8);
    uint16_t calc_crc = modbus_crc16(rx_buf, read - 2);
    if (rx_crc != calc_crc) {
        ESP_LOGW(TAG, "CRC mismatch: expected 0x%04X, got 0x%04X", calc_crc, rx_crc);
        return ESP_FAIL;
    }

    size_t payload = (size_t)read - 2;
    if (rx != NULL) {
        if (payload > rx_size) {
            return ESP_ERR_INVALID_SIZE;
        }
        memcpy(rx, rx_buf, payload);
    }
    if (rx_len != NULL) {
        *rx_len = payload;
    }
    return ESP_OK;
}

/**
 * @brief Send a Modbus request (CRC appended) and validate the response CRC
 */
esp_err_t modbus_send(uint8_t *cmd, size_t length)
{
    return modbus_transact(cmd, length, NULL, 0, NULL);
}

/**
 * @brief Read multiple holding registers (function code 03)
 */
esp_err_t modbus_read_registers(uint16_t addr, uint16_t *values, size_t count)
{
    uint8_t tx_buf[8];
    uint8_t rx_buf[MODBUS_BUF_SIZE];
    size_t rx_len = 0;

    if (values == NULL || count == 0 || count > 125) {
        return ESP_ERR_INVALID_ARG;
    }

    tx_buf[0] = MODBUS_SLAVE_ADDR;
    tx_buf[1] = 0x03;
    tx_buf[2] = addr >> 8;
    tx_buf[3] = addr & 0xFF;
    tx_buf[4] = count >> 8;
    tx_buf[5] = count & 0xFF;

    esp_err_t err = modbus_transact(tx_buf, 6, rx_buf, sizeof(rx_buf), &rx_len);
    if (err != ESP_OK) {
        return err;
    }

    if (rx_len >= 2 && (rx_buf[1] & 0x80)) {  /* exception response */
        ESP_LOGW(TAG, "Modbus exception code: %d", rx_len >= 3 ? rx_buf[2] : -1);
        return ESP_FAIL;
    }
    if (rx_len < 3 + count * 2 || rx_buf[1] != 0x03 || rx_buf[2] != count * 2) {
        ESP_LOGW(TAG, "Malformed response (len=%zu)", rx_len);
        return ESP_FAIL;
    }

    for (size_t i = 0; i < count; i++) {
        values[i] = ((uint16_t)rx_buf[3 + 2 * i] << 8) | rx_buf[4 + 2 * i];
    }
    return ESP_OK;
}

/**
 * @brief Read a single holding register (function code 03)
 */
esp_err_t modbus_read_register(uint16_t address, uint16_t *value)
{
    return modbus_read_registers(address, value, 1);
}

/**
 * @brief Write a single holding register (function code 06)
 */
esp_err_t modbus_write_register(uint16_t address, uint16_t value)
{
    uint8_t tx_buf[8];
    uint8_t rx_buf[8];
    size_t rx_len = 0;

    tx_buf[0] = MODBUS_SLAVE_ADDR;
    tx_buf[1] = 0x06;
    tx_buf[2] = address >> 8;
    tx_buf[3] = address & 0xFF;
    tx_buf[4] = value >> 8;
    tx_buf[5] = value & 0xFF;

    esp_err_t err = modbus_transact(tx_buf, 6, rx_buf, sizeof(rx_buf), &rx_len);
    if (err != ESP_OK) {
        return err;
    }
    if (rx_len >= 2 && (rx_buf[1] & 0x80)) {
        ESP_LOGW(TAG, "Modbus exception code: %d", rx_len >= 3 ? rx_buf[2] : -1);
        return ESP_FAIL;
    }
    return ESP_OK;
}

/**
 * @brief Send a raw Modbus PDU (slave address prepended, CRC appended)
 */
esp_err_t modbus_send_pdu(uint8_t fc, uint16_t addr,
                          const uint8_t *data, size_t data_len)
{
    uint8_t tx_buf[MODBUS_BUF_SIZE];

    if (data_len > MODBUS_BUF_SIZE - 6) {
        return ESP_ERR_INVALID_SIZE;
    }

    tx_buf[0] = MODBUS_SLAVE_ADDR;
    tx_buf[1] = fc;
    tx_buf[2] = addr >> 8;
    tx_buf[3] = addr & 0xFF;
    if (data != NULL && data_len > 0) {
        memcpy(&tx_buf[4], data, data_len);
    }

    return modbus_transact(tx_buf, 4 + data_len, NULL, 0, NULL);
}

/**
 * @brief Read two consecutive registers as a 32-bit unsigned integer (big-endian)
 */
esp_err_t modbus_read_uint32(uint16_t address, uint32_t *value)
{
    uint8_t tx_buf[8];
    uint8_t rx_buf[MODBUS_BUF_SIZE];
    size_t rx_len = 0;

    if (value == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    tx_buf[0] = MODBUS_SLAVE_ADDR;
    tx_buf[1] = 0x03;
    tx_buf[2] = address >> 8;
    tx_buf[3] = address & 0xFF;
    tx_buf[4] = 2 >> 8;  /* quantity = 2 */
    tx_buf[5] = 2 & 0xFF;

    esp_err_t err = modbus_transact(tx_buf, 6, rx_buf, sizeof(rx_buf), &rx_len);
    if (err != ESP_OK) {
        return err;
    }

    if (rx_len >= 2 && (rx_buf[1] & 0x80)) {  /* exception response */
        ESP_LOGW(TAG, "Modbus exception code: %d", rx_len >= 3 ? rx_buf[2] : -1);
        return ESP_FAIL;
    }
    if (rx_len < 3 + 2 * 2 || rx_buf[1] != 0x03 || rx_buf[2] != 4) {
        ESP_LOGW(TAG, "Malformed response (len=%zu)", rx_len);
        return ESP_FAIL;
    }

    *value = ((uint32_t)rx_buf[3] << 24) | ((uint32_t)rx_buf[4] << 16) |
            ((uint32_t)rx_buf[5] << 8) | rx_buf[6];
    return ESP_OK;
}

/**
 * @brief Read two consecutive registers as an IEEE754 single-precision float
 */
esp_err_t modbus_read_float(uint16_t address, float *value)
{
    uint32_t uint32_val;
    esp_err_t err = modbus_read_uint32(address, &uint32_val);
    if (err != ESP_OK) {
        return err;
    }
    *value = *(const float *)&uint32_val;  /* union-style cast, no extra code */
    return ESP_OK;
}

/**
 * @brief Read four consecutive registers as an IEEE754 double-precision float
 */
esp_err_t modbus_read_double(uint16_t address, double *value)
{
    uint8_t tx_buf[8];
    uint8_t rx_buf[MODBUS_BUF_SIZE];
    size_t rx_len = 0;

    if (value == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    tx_buf[0] = MODBUS_SLAVE_ADDR;
    tx_buf[1] = 0x03;
    tx_buf[2] = address >> 8;
    tx_buf[3] = address & 0xFF;
    tx_buf[4] = 4 >> 8;  /* quantity = 4 */
    tx_buf[5] = 4 & 0xFF;

    esp_err_t err = modbus_transact(tx_buf, 6, rx_buf, sizeof(rx_buf), &rx_len);
    if (err != ESP_OK) {
        return err;
    }

    if (rx_len >= 2 && (rx_buf[1] & 0x80)) {  /* exception response */
        ESP_LOGW(TAG, "Modbus exception code: %d", rx_len >= 3 ? rx_buf[2] : -1);
        return ESP_FAIL;
    }
    if (rx_len < 3 + 4 * 2 || rx_buf[1] != 0x03 || rx_buf[2] != 8) {
        ESP_LOGW(TAG, "Malformed response (len=%zu)", rx_len);
        return ESP_FAIL;
    }

    uint64_t uint64_val = ((uint64_t)rx_buf[3] << 56) | ((uint64_t)rx_buf[4] << 48) |
                          ((uint64_t)rx_buf[5] << 40) | ((uint64_t)rx_buf[6] << 32) |
                          ((uint64_t)rx_buf[7] << 24) | ((uint64_t)rx_buf[8] << 16) |
                          ((uint64_t)rx_buf[9] << 8) | rx_buf[10];
    *value = *(const double *)&uint64_val;
    return ESP_OK;
}

/**
 * @brief Write a 32-bit unsigned integer across two consecutive registers (big-endian)
 */
esp_err_t modbus_write_uint32(uint16_t address, uint32_t value)
{
    uint8_t tx_buf[8];

    tx_buf[0] = MODBUS_SLAVE_ADDR;
    tx_buf[1] = 0x10;  /* function code 10: write multiple registers */
    tx_buf[2] = address >> 8;
    tx_buf[3] = address & 0xFF;
    tx_buf[4] = 2 >> 8;  /* quantity = 2 */
    tx_buf[5] = 2 & 0xFF;
    tx_buf[6] = (value >> 24) & 0xFF;
    tx_buf[7] = (value >> 16) & 0xFF;
    tx_buf[8] = (value >> 8) & 0xFF;
    tx_buf[9] = value & 0xFF;

    return modbus_transact(tx_buf, 10, NULL, 0, NULL);
}

/**
 * @brief Write an IEEE754 single-precision float across two consecutive registers
 */
esp_err_t modbus_write_float(uint16_t address, float value)
{
    uint32_t uint32_val;
    memcpy(&uint32_val, &value, sizeof(float));
    return modbus_write_uint32(address, uint32_val);
}

/**
 * @brief Write an IEEE754 double-precision float across four consecutive registers
 */
esp_err_t modbus_write_double(uint16_t address, double value)
{
    uint8_t tx_buf[14];

    tx_buf[0] = MODBUS_SLAVE_ADDR;
    tx_buf[1] = 0x10;  /* function code 10: write multiple registers */
    tx_buf[2] = address >> 8;
    tx_buf[3] = address & 0xFF;
    tx_buf[4] = 4 >> 8;  /* quantity = 4 */
    tx_buf[5] = 4 & 0xFF;
    tx_buf[6] = (uint64_t)value >> 56;
    tx_buf[7] = ((uint64_t)value >> 48) & 0xFF;
    tx_buf[8] = ((uint64_t)value >> 40) & 0xFF;
    tx_buf[9] = ((uint64_t)value >> 32) & 0xFF;
    tx_buf[10] = ((uint64_t)value >> 24) & 0xFF;
    tx_buf[11] = ((uint64_t)value >> 16) & 0xFF;
    tx_buf[12] = ((uint64_t)value >> 8) & 0xFF;
    tx_buf[13] = (uint64_t)value & 0xFF;

    return modbus_transact(tx_buf, 14, NULL, 0, NULL);
}
