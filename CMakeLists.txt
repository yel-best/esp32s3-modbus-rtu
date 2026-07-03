# ESP-IDF CMakeLists.txt for ESP32-S3 Modbus RTU Project

idf_component_register(
    SRCS "src/main.c" "src/modbus.c" "src/wifi.c" "src/mqtt.c"
    INCLUDE_DIRS "include"
    REQUIRES esp_modem esp_http_client esp_event esp_wifi esp_netif mqtt
)
