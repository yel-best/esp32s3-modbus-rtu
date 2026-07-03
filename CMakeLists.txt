# ESP-IDF CMakeLists.txt for ESP32-S3 Modbus RTU Project

idf_component_register(
    SRCS "src/main.c" "src/modbus.c" "src/wifi.c" "src/mqtt.c"
    INCLUDE_DIRS "include"
    REQUIRES esp_modem esp_http_client esp_event esp_wifi esp_netif mqtt unity
)

# 添加测试组件
idf_component_register(
    SRCS "src/test/modbus_test.c" "src/test/mqtt_test.c" "src/test/wifi_test.c"
    INCLUDE_DIRS "include"
    REQUIRES unity
    LIBS "-lunity"
)
