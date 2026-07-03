/**
 * @file modbus_test.c
 * @brief Unity-based unit tests for Modbus RTU component
 */

#include "unity.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "modbus.h"
#include "driver/uart.h"

// Mock UART handle for testing
static uart_handle_t mock_uart_handle = NULL;

void setUp(void) {
    // Initialize mock UART
    mock_uart_handle = UART_HANDLE(UART_NUM_0);
}

void tearDown(void) {
    // Clean up mock state
}

/**
 * @brief Test modbus_crc16 function with known vectors
 */
void test_modbus_crc16_basic(void) {
    uint8_t data[] = {0x03, 0x00, 0x01, 0x00, 0x01};
    uint16_t crc = modbus_crc16(data, 5);
    TEST_ASSERT_EQUAL(0x94B3, crc);
}

/**
 * @brief Test modbus_crc16 with empty data
 */
void test_modbus_crc16_empty(void) {
    uint8_t data[] = {};
    uint16_t crc = modbus_crc16(data, 0);
    TEST_ASSERT_EQUAL(0xFFFF, crc);
}

/**
 * @brief Test modbus_read_req_build function
 */
void test_modbus_read_req_build(void) {
    uint8_t tx_buf[256];
    size_t tx_len;
    
    esp_err_t err = modbus_build_read_req(0, 1, tx_buf, &tx_len);
    TEST_ASSERT_EQUAL(ESP_OK, err);
    TEST_ASSERT_EQUAL(5, tx_len);
    
    // Verify function code and address
    TEST_ASSERT_EQUAL(0x03, tx_buf[0]);
    TEST_ASSERT_EQUAL(0x00, tx_buf[1]);  // Address high byte
    TEST_ASSERT_EQUAL(0x01, tx_buf[2]);  // Address low byte
    TEST_ASSERT_EQUAL(0x00, tx_buf[3]);  // Quantity high byte
    TEST_ASSERT_EQUAL(0x01, tx_buf[4]);  // Quantity low byte
}

/**
 * @brief Test modbus_read_req_build with larger address and quantity
 */
void test_modbus_read_req_build_large(void) {
    uint8_t tx_buf[256];
    size_t tx_len;
    
    esp_err_t err = modbus_build_read_req(0x1234, 5, tx_buf, &tx_len);
    TEST_ASSERT_EQUAL(ESP_OK, err);
    TEST_ASSERT_EQUAL(5, tx_len);
    
    // Verify function code and address
    TEST_ASSERT_EQUAL(0x03, tx_buf[0]);
    TEST_ASSERT_EQUAL(0x12, tx_buf[1]);  // Address high byte
    TEST_ASSERT_EQUAL(0x34, tx_buf[2]);  // Address low byte
    TEST_ASSERT_EQUAL(0x00, tx_buf[3]);  // Quantity high byte
    TEST_ASSERT_EQUAL(0x05, tx_buf[4]);  // Quantity low byte
}

/**
 * @brief Test modbus_build_write_req function
 */
void test_modbus_build_write_req(void) {
    uint8_t tx_buf[256];
    size_t tx_len;
    
    esp_err_t err = modbus_build_write_req(0x1234, 0xABCD, tx_buf, &tx_len);
    TEST_ASSERT_EQUAL(ESP_OK, err);
    TEST_ASSERT_EQUAL(5, tx_len);
    
    // Verify function code and address
    TEST_ASSERT_EQUAL(0x06, tx_buf[0]);  // Function code 06
    TEST_ASSERT_EQUAL(0x12, tx_buf[1]);  // Address high byte
    TEST_ASSERT_EQUAL(0x34, tx_buf[2]);  // Address low byte
    TEST_ASSERT_EQUAL(0xAB, tx_buf[3]);  // Value high byte
    TEST_ASSERT_EQUAL(0xCD, tx_buf[4]);  // Value low byte
}

/**
 * @brief Test modbus_crc16 with longer data
 */
void test_modbus_crc16_longer(void) {
    uint8_t data[] = {0x03, 0x00, 0x01, 0x00, 0x05, 0x00, 0x0A, 0x00, 0x14};
    uint16_t crc = modbus_crc16(data, 9);
    TEST_ASSERT_EQUAL(0x8D36, crc);
}

void main(void) { }
