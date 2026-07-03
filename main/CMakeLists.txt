idf_component_register(
    SRCS "main.c" "modbus.c" "wifi.c" "mqtt.c"
    INCLUDE_DIRS "include"
    PRIV_REQUIRES driver esp_wifi esp_netif esp_event nvs_flash mqtt
)
