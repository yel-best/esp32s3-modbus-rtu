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

/*
 * Known CRC values (computed with LSB-first poly 0xA001, init 0xFFFF):
 *
 * Function code 3, address 0x0000, count 1:
 *   Payload: 03 00 00 00 01 -> CRC 0x94B3
 *
 * Function code 3, address 0x1234, count 5:
 *   Payload: 03 12 34 00 05 -> CRC 0x8D36
 *
 * Function code 6, address 0x1234, value 0xABCD:
 *   Payload: 06 12 34 AB CD -> CRC 0x3F7E
 *
 * Function code 10 (write multiple), address 0x1234, count 2, values 0x0001 0x0002:
 *   Payload: 10 12 34 00 02 00 01 00 02 -> CRC 0x8C65
 *
 * Function code 10, address 0x1234, count 4, value 0x3FF00000 (float 1.0f):
 *   Payload: 10 12 34 00 04 3F F0 00 00 -> CRC 0x9C76
 */

/**
 * @brief Test modbus_read_uint32 with a known vector
 */
void test_modbus_read_uint32(void)
{
    uint8_t tx_buf[8] = {0x01, 0x03, 0x00, 0x00, 0x00, 0x02};
    uint8_t rx_buf[MODBUS_BUF_SIZE] = {
        0x01, 0x03, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x01, 0x02, 0x94B3 & 0xFF,
        (uint8_t)(0x94B3 >> 8)
    };
    uint32_t value = 0x01020304;

    uint8_t *rx_ptr = rx_buf;
    size_t rx_len = sizeof(rx_buf);
    esp_err_t err = modbus_transact(tx_buf, 6, rx_ptr, rx_len, &rx_len);

    TEST_ASSERT_EQUAL(ESP_OK, err);
    TEST_ASSERT_EQUAL(10, rx_len);  /* addr + fc + 4 bytes + CRC */

    uint16_t crc = (uint16_t)rx_buf[rx_len - 2] | ((uint16_t)rx_buf[rx_len - 1] << 8);
    uint16_t calc_crc = modbus_crc16(rx_buf, rx_len - 2);
    TEST_ASSERT_EQUAL(0x94B3, crc);
    TEST_ASSERT_EQUAL(calc_crc, crc);

    /* Extract big-endian uint32 */
    uint32_t extracted = ((uint32_t)rx_buf[3] << 24) | ((uint32_t)rx_buf[4] << 16) |
                         ((uint32_t)rx_buf[5] << 8) | rx_buf[6];
    TEST_ASSERT_EQUAL(value, extracted);
}

/**
 * @brief Test modbus_read_float with a known vector (float 1.0f)
 */
void test_modbus_read_float(void)
{
    uint8_t tx_buf[8] = {0x01, 0x03, 0x00, 0x00, 0x00, 0x02};
    uint8_t rx_buf[MODBUS_BUF_SIZE] = {
        0x01, 0x03, 0x00, 0x00, 0x00, 0x04,
        0x00, 0xF0, 0x3F, 0x00, 0x00, 0x9C76 & 0xFF,
        (uint8_t)(0x9C76 >> 8)
    };
    float value = 1.0f;

    uint8_t *rx_ptr = rx_buf;
    size_t rx_len = sizeof(rx_buf);
    esp_err_t err = modbus_transact(tx_buf, 6, rx_ptr, rx_len, &rx_len);

    TEST_ASSERT_EQUAL(ESP_OK, err);
    TEST_ASSERT_EQUAL(10, rx_len);

    uint16_t crc = (uint16_t)rx_buf[rx_len - 2] | ((uint16_t)rx_buf[rx_len - 1] << 8);
    uint16_t calc_crc = modbus_crc16(rx_buf, rx_len - 2);
    TEST_ASSERT_EQUAL(0x9C76, crc);
    TEST_ASSERT_EQUAL(calc_crc, crc);

    /* Extract big-endian uint32 then cast to float */
    uint32_t uint32_val = ((uint32_t)rx_buf[3] << 24) | ((uint32_t)rx_buf[4] << 16) |
                          ((uint32_t)rx_buf[5] << 8) | rx_buf[6];
    float extracted = *(const float *)&uint32_val;
    TEST_ASSERT_EQUAL(value, extracted);
}

/**
 * @brief Test modbus_read_double with a known vector (double 1.0)
 */
void test_modbus_read_double(void)
{
    uint8_t tx_buf[8] = {0x01, 0x03, 0x00, 0x00, 0x00, 0x04};
    uint8_t rx_buf[MODBUS_BUF_SIZE] = {
        0x01, 0x03, 0x00, 0x00, 0x00, 0x08,
        0x00, 0x00, 0xF0, 0x3F, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x9A75 & 0xFF,
        (uint8_t)(0x9A75 >> 8)
    };
    double value = 1.0;

    uint8_t *rx_ptr = rx_buf;
    size_t rx_len = sizeof(rx_buf);
    esp_err_t err = modbus_transact(tx_buf, 6, rx_ptr, rx_len, &rx_len);

    TEST_ASSERT_EQUAL(ESP_OK, err);
    TEST_ASSERT_EQUAL(14, rx_len);  /* addr + fc + 8 bytes + CRC */

    uint16_t crc = (uint16_t)rx_buf[rx_len - 2] | ((uint16_t)rx_buf[rx_len - 1] << 8);
    uint16_t calc_crc = modbus_crc16(rx_buf, rx_len - 2);
    TEST_ASSERT_EQUAL(0x9A75, crc);
    TEST_ASSERT_EQUAL(calc_crc, crc);

    /* Extract big-endian uint64 then cast to double */
    uint64_t uint64_val = ((uint64_t)rx_buf[3] << 56) | ((uint64_t)rx_buf[4] << 48) |
                          ((uint64_t)rx_buf[5] << 40) | ((uint64_t)rx_buf[6] << 32) |
                          ((uint64_t)rx_buf[7] << 24) | ((uint64_t)rx_buf[8] << 16) |
                          ((uint64_t)rx_buf[9] << 8) | rx_buf[10];
    double extracted = *(const double *)&uint64_val;
    TEST_ASSERT_EQUAL(value, extracted);
}

/**
 * @brief Test modbus_write_uint32 with a known vector
 */
void test_modbus_write_uint32(void)
{
    uint8_t tx_buf[10] = {0x01, 0x10, 0x12, 0x34, 0x00, 0x02, 0x00, 0x01, 0x00, 0x02};
    uint8_t rx_buf[MODBUS_BUF_SIZE] = {
        0x01, 0x10, 0x12, 0x34, 0x00, 0x02, 0x00, 0x01, 0x00, 0x02,
        0x8C65 & 0xFF,
        (uint8_t)(0x8C65 >> 8)
    };

    uint8_t *rx_ptr = rx_buf;
    size_t rx_len = sizeof(rx_buf);
    esp_err_t err = modbus_transact(tx_buf, 10, rx_ptr, rx_len, &rx_len);

    TEST_ASSERT_EQUAL(ESP_OK, err);
    TEST_ASSERT_EQUAL(12, rx_len);  /* echo back + CRC */

    uint16_t crc = (uint16_t)rx_buf[rx_len - 2] | ((uint16_t)rx_buf[rx_len - 1] << 8);
    uint16_t calc_crc = modbus_crc16(rx_buf, rx_len - 2);
    TEST_ASSERT_EQUAL(0x8C65, crc);
    TEST_ASSERT_EQUAL(calc_crc, crc);
}

/**
 * @brief Test modbus_write_float with a known vector (float 1.0f)
 */
void test_modbus_write_float(void)
{
    uint8_t tx_buf[10] = {0x01, 0x10, 0x12, 0x34, 0x00, 0x02, 0x3F, 0xF0, 0x00, 0x00};
    uint8_t rx_buf[MODBUS_BUF_SIZE] = {
        0x01, 0x10, 0x12, 0x34, 0x00, 0x02, 0x3F, 0xF0, 0x00, 0x00,
        0x9C76 & 0xFF,
        (uint8_t)(0x9C76 >> 8)
    };

    uint8_t *rx_ptr = rx_buf;
    size_t rx_len = sizeof(rx_buf);
    esp_err_t err = modbus_transact(tx_buf, 10, rx_ptr, rx_len, &rx_len);

    TEST_ASSERT_EQUAL(ESP_OK, err);
    TEST_ASSERT_EQUAL(12, rx_len);

    uint16_t crc = (uint16_t)rx_buf[rx_len - 2] | ((uint16_t)rx_buf[rx_len - 1] << 8);
    uint16_t calc_crc = modbus_crc16(rx_buf, rx_len - 2);
    TEST_ASSERT_EQUAL(0x9C76, crc);
    TEST_ASSERT_EQUAL(calc_crc, crc);
}

/**
 * @brief Test modbus_write_double with a known vector (double 1.0)
 */
void test_modbus_write_double(void)
{
    uint8_t tx_buf[14] = {
        0x01, 0x10, 0x12, 0x34, 0x00, 0x04,
        0x00, 0x00, 0xF0, 0x3F, 0x00, 0x00, 0x00, 0x00
    };
    uint8_t rx_buf[MODBUS_BUF_SIZE] = {
        0x01, 0x10, 0x12, 0x34, 0x00, 0x04,
        0x00, 0x00, 0xF0, 0x3F, 0x00, 0x00, 0x00, 0x00,
        0x9A75 & 0xFF,
        (uint8_t)(0x9A75 >> 8)
    };

    uint8_t *rx_ptr = rx_buf;
    size_t rx_len = sizeof(rx_buf);
    esp_err_t err = modbus_transact(tx_buf, 14, rx_ptr, rx_len, &rx_len);

    TEST_ASSERT_EQUAL(ESP_OK, err);
    TEST_ASSERT_EQUAL(16, rx_len);

    uint16_t crc = (uint16_t)rx_buf[rx_len - 2] | ((uint16_t)rx_buf[rx_len - 1] << 8);
    uint16_t calc_crc = modbus_crc16(rx_buf, rx_len - 2);
    TEST_ASSERT_EQUAL(0x9A75, crc);
    TEST_ASSERT_EQUAL(calc_crc, crc);
}

void main(void) { }
