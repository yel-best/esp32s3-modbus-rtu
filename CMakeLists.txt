# ESP-IDF CMakeLists.txt for ESP32-S3 Modbus RTU Project

idf_component_register(
    SRCS "src/main.c"
    INCLUDE_DIRS "."
    REQUIRES esp_modem esp_http_client esp_event esp_wifi esp_netif
)
