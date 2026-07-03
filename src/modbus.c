/**
 * @file modbus.c
 * @brief Modbus RTU implementation for ESP32-S3
 * 
 * Provides serial-based Modbus RTU communication support.
 */

#include <string.h>
#include "esp_log.h"
#include "esp_err.h"
#include "driver/uart.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "modbus.h"

#define TAG "MODBUS"
#define MODBUS_TX_BUF_SIZE 256
#define MODBUS_RX_BUF_SIZE 256

static uart_handle_t uart_modbus_handle;
static QueueHandle_t modbus_rx_queue = NULL;
static TaskHandle_t modbus_task_handle = NULL;
static bool modbus_initialized = false;

/**
 * @brief Initialize Modbus RTU serial interface
 */
void modbus_init(void)
{
    if (modbus_initialized) {
        ESP_LOGW(TAG, "Modbus already initialized");
        return;
    }

    ESP_LOGI(TAG, "Initializing Modbus RTU on UART...");

    uart_config_t uart_config = {
        .baud_rate = 9600,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };

    // Configure UART0 as Modbus RTU interface
    ESP_ERROR_CHECK(uart_param_config(UART_NUM_0, &uart_config));
    ESP_ERROR_CHECK(uart_driver_install(UART_NUM_0, MODBUS_TX_BUF_SIZE, 0, 0, NULL, 0));
    
    uart_modbus_handle = UART_HANDLE(UART_NUM_0);

    // Create queue for receiving Modbus responses
    modbus_rx_queue = xQueueCreate(MODBUS_RX_BUF_SIZE, sizeof(uint8_t));
    if (modbus_rx_queue == NULL) {
        ESP_LOGE(TAG, "Failed to create Modbus RX queue");
        return;
    }

    modbus_initialized = true;
    ESP_LOGI(TAG, "Modbus RTU initialized on UART0 (9600bps)");
}

/**
 * @brief Run Modbus RTU task (stub - implement actual task logic here)
 */
static void modbus_task(void *arg)
{
    ESP_LOGI(TAG, "Modbus RTU task started");
    // TODO: Implement Modbus task loop with proper event processing
    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(1000));  // Process periodically
    }
}

/**
 * @brief Start Modbus RTU task
 */
void modbus_start(void)
{
    if (modbus_initialized) {
        ESP_ERROR_CHECK(xTaskCreate(modbus_task, "MODBUS", 2048, NULL, 3, &modbus_task_handle));
    }
}

/**
 * @brief Build and send a Modbus Read Holding Registers request (function code 03)
 * @param addr Register address to read
 * @param quantity Number of registers to read
 * @param tx_buf Output buffer for the command (max 256 bytes)
 * @param tx_len Output: length of command sent
 * @return ESP_OK on success, ESP_ERR_INVALID_ARG if parameters invalid
 */
static esp_err_t modbus_build_read_req(uint16_t addr, uint8_t quantity, 
                                       uint8_t *tx_buf, size_t *tx_len)
{
    // Function code 03: Read Holding Registers
    tx_buf[0] = 0x03;           // Function code
    tx_buf[1] = (addr >> 8) & 0xFF;
    tx_buf[2] = addr & 0xFF;
    tx_buf[3] = (quantity >> 8) & 0xFF;
    tx_buf[4] = quantity & 0xFF;
    *tx_len = 5;
    return ESP_OK;
}

/**
 * @brief Receive Modbus response from UART using queue (non-blocking)
 * @param buf Buffer for received data
 * @param max_len Maximum length to receive
 * @return Actual bytes received (including CRC), ESP_FAIL on error
 */
static esp_err_t modbus_recv(uint8_t *buf, size_t max_len)
{
    uint8_t rx_buf[256];
    
    // Receive from queue with timeout (1s)
    if (xQueueReceive(modbus_rx_queue, rx_buf, pdMS_TO_TICKS(1000)) != pdTRUE) {
        ESP_LOGW(TAG, "No data received from Modbus device");
        return ESP_FAIL;
    }
    
    size_t rx_len = (size_t)xQueueGetFront(modbus_rx_queue);
    if (rx_len == 0 || rx_len > max_len) {
        ESP_LOGW(TAG, "Invalid received length: %zu", rx_len);
        return ESP_FAIL;
    }
    
    // Validate CRC
    uint16_t crc = ((uint16_t)rx_buf[rx_len - 2] << 8) | rx_buf[rx_len - 1];
    uint16_t tx_crc = modbus_crc16(rx_buf, rx_len - 2);
    
    if (crc != tx_crc) {
        ESP_LOGW(TAG, "CRC mismatch: expected 0x%04X, got 0x%04X", tx_crc, crc);
        return ESP_FAIL;
    }
    
    // Copy to output buffer
    memcpy(buf, rx_buf, rx_len);
    return ESP_OK;
}

/**
 * @brief Read multiple Modbus registers
 * @param addr Start register address
 * @param values Array to store read values
 * @param count Number of registers to read
 * @return ESP_OK on success, ESP_ERR_INVALID_ARG if parameters invalid, ESP_FAIL if no response
 */
esp_err_t modbus_read_registers(uint16_t addr, uint16_t *values, size_t count)
{
    uint8_t tx_buf[256];
    uint8_t rx_buf[256];
    size_t tx_len = 0;
    size_t rx_len = 0;
    uint8_t fc = 0x03;          // Read Holding Registers
    uint16_t data_count = 0;
    uint16_t error_code = 0;
    
    if (addr > 65534 || count == 0) {
        return ESP_ERR_INVALID_ARG;
    }
    
    ESP_LOGD(TAG, "Reading %zu registers starting at address %d", count, addr);
    
    // Build request: Function code + Address + Quantity
    tx_buf[0] = fc;            // Function code 03
    tx_buf[1] = (addr >> 8) & 0xFF;
    tx_buf[2] = addr & 0xFF;
    tx_buf[3] = (count >> 8) & 0xFF;
    tx_buf[4] = count & 0xFF;
    tx_len = 5;
    
    // Send request with CRC
    if (!modbus_send(tx_buf, tx_len)) {
        ESP_LOGW(TAG, "No response from Modbus device");
        return ESP_FAIL;
    }
    
    // Receive response
    esp_err_t recv_err = modbus_recv(rx_buf, sizeof(rx_buf));
    if (recv_err != ESP_OK || rx_len < 5) {
        ESP_LOGW(TAG, "Invalid response length: %zu", rx_len);
        return ESP_FAIL;
    }
    
    fc = rx_buf[0];
    if (fc != 0x03) {
        ESP_LOGW(TAG, "Unexpected function code: 0x%02X", fc);
        return ESP_FAIL;
    }
    
    error_code = rx_buf[1];
    if (error_code != 0) {
        ESP_LOGW(TAG, "Modbus error code: %d", error_code);
        return ESP_ERR_INVALID_ARG;
    }
    
    data_count = (rx_buf[2] << 8) | rx_buf[3];
    if (data_count < count) {
        ESP_LOGW(TAG, "Expected %zu registers, got %d", count, data_count);
        return ESP_ERR_INVALID_ARG;
    }
    
    // Copy register values (big-endian)
    for (size_t i = 0; i < count; i++) {
        values[i] = (rx_buf[4 + 2*i] << 8) | rx_buf[5 + 2*i];
    }
    
    ESP_LOGD(TAG, "Read %zu registers successfully", count);
    return ESP_OK;
}

/**
 * @brief Read a Modbus register value
 * @param address Register address
 * @param value Pointer to store the read value
 * @return ESP_OK on success, ESP_ERR_INVALID_ARG if address invalid, ESP_FAIL if no response
 */
esp_err_t modbus_read_register(uint16_t address, uint16_t *value)
{
    uint8_t tx_buf[256];
    uint8_t rx_buf[256];
    size_t tx_len = 0;
    size_t rx_len = 0;
    uint8_t fc = 0x03;          // Read Holding Registers
    uint8_t error_code = 0;
    uint16_t data_count = 0;
    
    if (address > 65534) {
        return ESP_ERR_INVALID_ARG;
    }
    
    ESP_LOGD(TAG, "Reading register %d", address);
    
    // Build request
    if (modbus_build_read_req(address, 1, tx_buf, &tx_len) != ESP_OK) {
        return ESP_ERR_INVALID_ARG;
    }
    
    // Send request with CRC
    if (!modbus_send(tx_buf, tx_len)) {
        ESP_LOGW(TAG, "No response from Modbus device");
        return ESP_FAIL;
    }
    
    // Receive response
    esp_err_t recv_err = modbus_recv(rx_buf, sizeof(rx_buf));
    if (recv_err != ESP_OK || rx_len < 5) {
        ESP_LOGW(TAG, "Invalid response length: %zu", rx_len);
        return ESP_FAIL;
    }
    
    fc = rx_buf[0];
    if (fc != 0x03) {
        ESP_LOGW(TAG, "Unexpected function code: 0x%02X", fc);
        return ESP_FAIL;
    }
    
    error_code = rx_buf[1];
    if (error_code != 0) {
        ESP_LOGW(TAG, "Modbus error code: %d", error_code);
        return ESP_ERR_INVALID_ARG;
    }
    
    data_count = (rx_buf[2] << 8) | rx_buf[3];
    if (data_count < 1) {
        ESP_LOGW(TAG, "Invalid data count: %d", data_count);
        return ESP_FAIL;
    }
    
    // Extract register value (big-endian)
    *value = (rx_buf[4] << 8) | rx_buf[5];
    ESP_LOGD(TAG, "Read register %d = %d", address, *value);
    
    return ESP_OK;
}

/**
 * @brief Build and send a Modbus Write Single Register request (function code 06)
 * @param addr Register address to write
 * @param val Value to write
 * @param tx_buf Output buffer for the command (max 256 bytes)
 * @param tx_len Output: length of command sent
 * @return ESP_OK on success, ESP_ERR_INVALID_ARG if parameters invalid
 */
static esp_err_t modbus_build_write_req(uint16_t addr, uint16_t val,
                                        uint8_t *tx_buf, size_t *tx_len)
{
    // Function code 06: Write Single Register
    tx_buf[0] = 0x06;           // Function code
    tx_buf[1] = (addr >> 8) & 0xFF;
    tx_buf[2] = addr & 0xFF;
    tx_buf[3] = (val >> 8) & 0xFF;
    tx_buf[4] = val & 0xFF;
    *tx_len = 5;
    return ESP_OK;
}

/**
 * @brief Write a Modbus register value
 * @param address Register address
 * @param value Value to write
 * @return ESP_OK on success, ESP_ERR_INVALID_ARG if address invalid, ESP_FAIL if no response
 */
esp_err_t modbus_write_register(uint16_t address, uint16_t value)
{
    uint8_t tx_buf[256];
    size_t tx_len = 0;
    
    if (address > 65534) {
        return ESP_ERR_INVALID_ARG;
    }
    
    ESP_LOGD(TAG, "Writing register %d = %d", address, value);
    
    // Build request
    if (modbus_build_write_req(address, value, tx_buf, &tx_len) != ESP_OK) {
        return ESP_ERR_INVALID_ARG;
    }
    
    // Send request with CRC
    return modbus_send(tx_buf, tx_len);
}

/**
 * @brief Send a Modbus PDU (Protocol Data Unit)
 * @param fc Function code
 * @param addr Register address
 * @param data Data bytes for the request
 * @param data_len Length of data
 * @return ESP_OK on success, ESP_ERR_INVALID_SIZE if data too large
 */
esp_err_t modbus_send_pdu(uint8_t fc, uint16_t addr,
                         const uint8_t *data, size_t data_len)
{
    uint8_t tx_buf[256];
    size_t tx_len;
    
    if (data_len > 254) {
        return ESP_ERR_INVALID_SIZE;
    }
    
    tx_buf[0] = fc;            // Function code
    tx_buf[1] = (addr >> 8) & 0xFF;
    tx_buf[2] = addr & 0xFF;
    tx_buf[3] = (data_len >> 8) & 0xFF;
    tx_buf[4] = data_len & 0xFF;
    
    if (data != NULL) {
        memcpy(&tx_buf[5], data, data_len);
    }
    
    // Calculate CRC16
    uint16_t crc = modbus_crc16(tx_buf, 5 + data_len);
    tx_buf[5 + data_len] = crc & 0xFF;
    tx_buf[6 + data_len] = crc >> 8;
    tx_len = 7 + data_len;
    
    // Send command with CRC
    return modbus_send(tx_buf, tx_len);
}

/**
 * @brief Calculate CRC16 for Modbus RTU
 * @param data Pointer to data to calculate CRC on
 * @param len Length of data
 * @return CRC16 value
 */
static uint16_t modbus_crc16(const uint8_t *data, size_t len)
{
    uint16_t crc = 0xFFFF;
    for (size_t i = 0; i < len; i++) {
        crc ^= ((uint16_t)data[i] << 8);
        for (int j = 0; j < 8; j++) {
            if (crc & 0x8000) {
                crc = (crc << 1) ^ 0xA001;
            } else {
                crc <<= 1;
            }
        }
    }
    return crc;
}

/**
 * @brief Send a Modbus request and receive response
 * @param cmd Pointer to Modbus command buffer (max 256 bytes)
 * @param length Length of command (without CRC)
 * @return ESP_OK on success, ESP_ERR_TIMEOUT if no response, ESP_FAIL on error
 */
esp_err_t modbus_send(uint8_t *cmd, size_t length)
{
    uint8_t tx_buf[256];
    size_t tx_len;
    uint8_t rx_buf[256];
    size_t rx_len;
    
    if (length == 0 || length > sizeof(tx_buf)) {
        return ESP_ERR_INVALID_SIZE;
    }
    
    // Calculate CRC16
    uint16_t crc = modbus_crc16(cmd, length);
    tx_buf[length] = crc & 0xFF;
    tx_buf[length + 1] = crc >> 8;
    tx_len = length + 2;
    
    // Send command with CRC
    ESP_LOGD(TAG, "Sending Modbus: %zu bytes", tx_len);
    if (uart_write_bytes(uart_modbus_handle, tx_buf, tx_len) != tx_len) {
        ESP_LOGW(TAG, "UART write failed");
        return ESP_FAIL;
    }
    
    // Receive response with timeout
    vTaskDelay(pdMS_TO_TICKS(100));
    rx_len = uart_read_bytes(uart_modbus_handle, rx_buf, sizeof(rx_buf), pdMS_TO_TICKS(500));
    
    if (rx_len == 0) {
        ESP_LOGW(TAG, "No response from Modbus device");
        return ESP_ERR_TIMEOUT;
    }
    
    // Validate CRC
    uint16_t rx_crc = ((uint16_t)rx_buf[rx_len - 2] << 8) | rx_buf[rx_len - 1];
    uint16_t tx_crc = modbus_crc16(cmd, length);
    
    if (rx_crc != tx_crc) {
        ESP_LOGW(TAG, "CRC mismatch: expected 0x%04X, got 0x%04X", tx_crc, rx_crc);
        return ESP_FAIL;
    }
    
    ESP_LOGD(TAG, "Received %zu bytes", rx_len);
    return ESP_OK;
}
